import math, re, os
import pprint

srcdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

keywords = {
  "break":       "T_BREAK",
  "case":        "T_CASE",
  "const":       "T_CONST",
  "continue":    "T_CONTINUE",
  "default":     "T_DEFAULT",
  "defer":       "T_DEFER",
  "else":        "T_ELSE",
  "enum":        "T_ENUM",
  "fallthrough": "T_FALLTHROUGH",
  "for":         "T_FOR",
  "fun":         "T_FUN",
  "go":          "T_GO",
  "if":          "T_IF",
  "import":      "T_IMPORT",
  "in":          "T_IN",
  "interface":   "T_INTERFACE",
  "is":          "T_IS",
  "return":      "T_RETURN",
  "select":      "T_SELECT",
  "struct":      "T_STRUCT",
  "switch":      "T_SWITCH",
  "symbol":      "T_SYMBOL",
  "type":        "T_TYPE",
  "var":         "T_VAR",
  "while":       "T_WHILE",
}

indentation = "  "
nodemap = {}  # key => C var
minkeylen = 1000000
maxkeylen = 0


def genBranch(pairs, output):
  leftPairs = None
  rightPairs = None

  if len(pairs) > 1:
    midIndex = math.floor(len(pairs) / 2) - 1
    k, v = pairs[midIndex]
    leftPairs = pairs[0:midIndex]
    rightPairs = pairs[midIndex + 1:]
  else:
    k, v = pairs[0]

  global minkeylen, maxkeylen
  minkeylen = min(minkeylen, len(k))
  maxkeylen = max(maxkeylen, len(k))

  s = [k, v]
  if leftPairs and len(leftPairs) > 0:
    s.append(genBranch(leftPairs, output))
  if rightPairs and len(rightPairs) > 0:
    s.append(genBranch(rightPairs, output))

  left  = ("&%s" % nodemap[s[2][0]]) if len(s) > 2 else "NULL"
  right = ("&%s" % nodemap[s[3][0]]) if len(s) > 3 else "NULL"

  output.append(
    "const BTreeNode %s = { %d, (const u8*)\"%s\", %s, %s, %s };" % (
      nodemap[k], len(k), k, v, left, right
    )
  )

  return s


def genSwitch(m, keys):
  prefixes = {}
  for k in sortedKeys:
    assert(len(k) > 1)
    c = k[0]
    ent = prefixes.get(c)
    if ent is None:
      prefixes[c] = [(k, m[k])]
    else:
      ent.append((k, m[k]))

  s = ['switch (c) {']
  for c, v in prefixes.items():
    s.append("case '%s':" % c)
  s.append('}')
  return s


def genBTree(m, sortedKeys, output):
  for k in sortedKeys:
    nodemap[k] = 'bn_%02d_%s' % (len(nodemap), re.sub(r'[^A-Za-z0-9_]+', "", k))

  print(pprint.pformat(genSwitch(m, sortedKeys), 2))

  prefixes = {}
  for k in sortedKeys:
    assert(len(k) > 1)
    c = k[0]
    ent = prefixes.get(c)
    if ent is None:
      prefixes[c] = [(k, m[k])]
    else:
      ent.append((k, m[k]))

  prefixKeys = sorted(prefixes.keys())
  print(pprint.pformat(prefixKeys, 2))

  pairs = []
  for k in sortedKeys:
    pairs.append((k, m[k]))

  return genBranch(pairs, output)


sortedKeys = list(keywords.keys())
sortedKeys.sort()

output = []
tree = genBTree(keywords, sortedKeys, output)

# print(minkeylen, maxkeylen)
print(pprint.pformat(tree, 2))

# defs = ["#define TOKEN_TYPES_KW(_) \\"]
# for k in sortedKeys:
#   defs.append("  _(%s) % 20s */ \\" % (keywords[k], "/* " + k))
# defs.append("/*END TOKEN_TYPES_KW*/")
# open(os.path.join(srcdir, 'src', 'token_kw.h'), 'w').write("\n".join(defs) + "\n")


# btree = output[:]
# btree.append("const BTreeNode* bn_root = &%s;" % nodemap[tree[0]])
# btree.append("const u32 KEYWORD_MINLEN = %d;" % minkeylen)
# btree.append("const u32 KEYWORD_MAXLEN = %d;" % maxkeylen)
# # print("\n".join(consts) + "\n" + pprint.pformat(output, 2))

# open(os.path.join(srcdir, 'src', 'scan_keywords_def.c'), 'w').write("\n".join(btree) + "\n")
# # print("\n".join(btree))

