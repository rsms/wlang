#
# -- This is just an idea--
#
# Rust-like borrowing and moving, with a twist.
# - Things are borrowed by default
# - Moving can be explicit
#

type Thing {
  x int
}

fun helperTakes(t Thing) {
  t.x = 2 # ok to mutate since we own t
}

fun f1 {
  t = Thing(1)
  t.x = 0 # ok to mutate since we own t
  helperTakesOver(t) # t moves
}

fun f2 {
  t = Thing(1)
  helperTakesOver(t)
  y = t # no longer alive; error: t moved to helperTakesOver
}

fun helperBorrows(t &Thing) {
  print(t.x) # reading is okay, but...
  # t.x = 2 # ...mutation is not, since we are just borrowing t
}

fun f3 {
  t = Thing(1)
  helperBorrows(t)
  t.x = 0 # ok to mutate since we still own t
}

fun f4 {
  # borrowing prevents moving
  t = Thing(1)
  y = &t # y borrows t
  helperTakes(t) # error: cannot move t; borrowed by y
}

fun f5 {
  # scope is important
  t = Thing(1)
  {
    y = &t # y borrows t
  }
  helperTakes(t) # ok; no borrowed refs in scope
}
