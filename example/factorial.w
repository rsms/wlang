# <
# <=
# <<
# <<=
# >
# >=
# >>
# >>=

fun main (arg1, arg0 int) {
  # a, b = 1, 2 + 1
  # z = 20 as int8
  # a = z as int16
  # a = int16(20)
  # b = int64(arg0)
  # k = x / y * z # oops! Right-associate but should be left-associative

  x = 1     # (Let x (Int 1))
  y = x + 3 as uint16 # (Let y (Op + (Ref x) (Int 2)))
  # if true y = x + 4
  y         # (Ref y)

  # factorial(start)
}

# fun foo(i int) -> i

# # Factorial function
# fun factorial(n int) int {
#   if n <= 0 {
#     1
#   } else {
#     n * factorial(n - 1)
#   }
# }

# fun factorial(n float32) float32 {
#   if n <= 0.0 {
#     1.0
#   } else {
#     n * factorial(n - 1.0)
#   }
# }

# fun factorial(n int) int {
#   # y = 3
#   # x, y, _ = 1, 2, 3
#   # t = (1, 2, 3)
#   # xs = for x in [1,2,3] { x * 2 }
#   # if n <= 0 1 else n * factorial(n - 1)
#   if n <= 0 {
#     1
#   } else {
#     n * factorial(n - 1)
#   }
# }
