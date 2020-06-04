// Resolve identifiers in an AST. Usuaully run right after parsing.
#include "wp.h"


typedef struct {
  CCtx* cc;
  u32   funNest;    // level of function nesting. 0 at file level
  u32   assignNest; // level of assignment. Used to avoid early constant folding in assignments.
} ResCtx;


static Node* resolve(Node* n, Scope* scope, ResCtx* ctx);


Node* ResolveSym(CCtx* cc, Node* n, Scope* scope) {
  ResCtx ctx = { cc, 0, 0 };
  return resolve(n, scope, &ctx);
}


static Node* resolveIdent(Node* n, Scope* scope, ResCtx* ctx) {
  assert(n->kind == NIdent);
  auto name = n->ref.name;
  // dlog("resolveIdent BEGIN %s", name);
  while (1) {
    auto target = n->ref.target;
    // dlog("  ITER %s", n->ref.name);

    if (target == NULL) {
      // dlog("  LOOKUP %s", n->ref.name);
      target = ScopeLookup(scope, n->ref.name);

      if (target == NULL) {
        CCtxErrorf(ctx->cc, n->pos, "undefined symbol %s", name);
        ((Node*)n)->ref.target = NodeBad;
        return n;
      }

      ((Node*)n)->ref.target = target;
      // dlog("  UNWIND %s => %s", n->ref.name, NodeKindName(target->kind));
    }

    // dlog("  SWITCH target %s", NodeKindName(target->kind));
    switch (target->kind) {
      case NIdent:
        // note: all built-ins which are const have targets, meaning the code above will
        // not mutate those nodes.
        n = (Node*)target;
        break; // continue unwind loop

      case NLet:
        // Unwind let bindings
        assert(target->field.init != NULL);
        if (NodeKindIsConst(target->field.init->kind)) {
          // in the case of a let target with a constant, resolve to the constant.
          // Example:
          //   x = true   # Identifier "true" is resolved to constant Boolean true
          //   y = x      # Identifier x is resolved to constant Boolean true via x
          //
          return target->field.init;
        }
        return n;

      case NBoolLit:
      case NIntLit:
      case NNil:
      case NFun:
      case NBasicType:
      case NTupleType:
      case NFunType:
        // unwind identifier to constant/immutable value.
        // Example:
        //   (Ident true #user) -> (Ident true #builtin) -> (Bool true #builtin)
        //
        // dlog("  RET target %s -> %s", NodeKindName(target->kind), NodeReprShort(target));
        if (ctx->assignNest > 0) {
          // assignNest is >0 when resolving the LHS of an assignment.
          // In this case we do not unwind constants as that would lead to things like this:
          //   (assign (tuple (ident a) (ident b)) (tuple (int 1) (int 2))) =>
          //   (assign (tuple (int 1) (int 2)) (tuple (int 1) (int 2)))
          return n;
        }
        return (Node*)target;

      default:
        assert(!NodeKindIsConst(target->kind)); // should be covered in case-statements above
        // dlog("resolveIdent FINAL %s => %s (target %s) type? %d",
        //   n->ref.name, NodeKindName(n->kind), NodeKindName(target->kind),
        //   NodeKindIsType(target->kind));
        // dlog("  RET n %s -> %s", NodeKindName(n->kind), NodeReprShort(n));
        return n;
    }
  }
}


static Node* resolve(Node* n, Scope* scope, ResCtx* ctx) {
  // dlog("resolve(%s, scope=%p)", NodeKindName(n->kind), scope);

  if (n->type != NULL && n->type->kind != NBasicType) {
    if (n->kind == NFun) {
      ctx->funNest++;
    }
    n->type = resolve((Node*)n->type, scope, ctx);
    if (n->kind == NFun) {
      ctx->funNest--;
    }
  }

  switch (n->kind) {

  // uses u.ref
  case NIdent: {
    return resolveIdent(n, scope, ctx);
  }

  // uses u.array
  case NBlock:
  case NTuple:
  case NFile: {
    if (n->array.scope) {
      scope = n->array.scope;
    }
    NodeListMap(&n->array.a, n,
      /* n = */ resolve(n, scope, ctx)
    );
    break;
  }

  // uses u.fun
  case NFun: {
    ctx->funNest++;
    if (n->fun.params) {
      n->fun.params = resolve(n->fun.params, scope, ctx);
    }
    if (n->type) {
      n->type = resolve(n->type, scope, ctx);
    }
    auto body = n->fun.body;
    if (body) {
      if (n->fun.scope) {
        scope = n->fun.scope;
      }
      n->fun.body = resolve(body, scope, ctx);
    }
    ctx->funNest--;
    break;
  }

  // uses u.op
  case NAssign: {
    ctx->assignNest++;
    resolve(n->op.left, scope, ctx);
    ctx->assignNest--;
    assert(n->op.right != NULL);
    n->op.right = resolve(n->op.right, scope, ctx);
    break;
  }
  case NOp:
  case NPrefixOp:
  case NReturn: {
    auto newleft = resolve(n->op.left, scope, ctx);
    if (n->op.left->kind != NIdent) {
      // note: in case of assignment where the left side is an identifier,
      // avoid replacing the identifier with its value.
      // This branch is taken in all other cases.
      n->op.left = newleft;
    }
    if (n->op.right) {
      n->op.right = resolve(n->op.right, scope, ctx);
    }
    break;
  }

  // uses u.call
  case NTypeCast:
    n->call.args = resolve(n->call.args, scope, ctx);
    n->call.receiver = resolve(n->call.receiver, scope, ctx);
    assert(NodeKindIsType(n->call.receiver->kind));
    break;

  case NCall:
    n->call.args = resolve(n->call.args, scope, ctx);
    auto recv = resolve(n->call.receiver, scope, ctx);
    n->call.receiver = recv;
    if (recv->kind != NFun) {
      // convert to type cast, if receiver is a type. e.g. "x = uint8(4)"
      // TODO: when type alises are implemented, add NTuple here to support:
      //       type Foo = (int, int); x = Foo(1,2)
      if (recv->kind == NBasicType) {
        if (NodeKindIsConst(n->call.args->kind)) {
          // TODO figure out a simple and elegant way to parse stuff like this:
          //
          //   x = int32(7)    =>  int32:(Let int32:(Ident x) int32:(Int 7))
          //   x = 7 as int32  =>  int32:(Let int32:(Ident x) int32:(Int 7))
          //
          // Currently;
          //   x = int32(7)    =>  int:(Let int:(Ident x) int32:(TypeCast int32 int32:(Int 7)))
          //   x = 7 as int32  =>  (not implemented)
          //
          dlog("type cast const. n->call.args = %s", NodeReprShort(n->call.args));
        }
        n->kind = NTypeCast;
      } else {
        CCtxErrorf(ctx->cc, n->pos, "cannot call %s", NodeReprShort(recv));
      }
    }
    break;

  // uses u.field
  case NLet:
  case NField: {
    if (n->field.init) {
      n->field.init = resolve(n->field.init, scope, ctx);
    }
    break;
  }

  // uses u.cond
  case NIf:
    n->cond.cond = resolve(n->cond.cond, scope, ctx);
    n->cond.thenb = resolve(n->cond.thenb, scope, ctx);
    if (n->cond.elseb) {
      n->cond.elseb = resolve(n->cond.elseb, scope, ctx);
    }
    break;

  case NNone:
  case NBad:
  case NBasicType:
  case NFunType:
  case NTupleType:
  case NComment:
  case NNil:
  case NBoolLit:
  case NIntLit:
  case NFloatLit:
  case NZeroInit:
  case _NodeKindMax:
    break;

  } // switch n->kind

  return n;
}

