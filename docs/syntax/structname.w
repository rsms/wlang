type Account { id, flag int } #T {flag:iid:i}
fun foo(a Account) { #T ^({flag:iid:i})i
	return 0 if a.flag == 0
	# ^ stmt if expr
	a.id
}
id, flag = 1, 0
x = { id, flag } #T {flag:iid:i}
foo(x) # ok
y = Account { id } # flag is zero init
foo(y) # ok
foo({ id }) # ok. flag is zero init
# inline type def:
fun bar(a { id int }) {...}
bar({ id }) # block expr or struct init?
type User {
	account { id, flag int } # inline type def
	name str
}
u = { name: "Sam", account: { id: 3 } }
foo(u.account) # ok

compile(callback, config) where
  config = Config{
	  infile: "foo.w",
	  debug: true,
  },
  callback = fun (ev Event) -> log(ev)


#----------------------

# struct exprs must be prefixed by type to
# disambiguate from block expr
a = { 3 } # block
b = Account { id: 3 } # struct
c = (type { id int }) { id: 3 } # struct
type Account { id int }

struct Account { id int }
type Foo = struct { id int }
c = (struct { id int }) { id: 3 } # struct

type Account = { id int }
c = (type _ = { id int }) { id: 3 }


