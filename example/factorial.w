start = true
start = 3  # shadowing

fun main {
  # a, b = 10, 20
  x = 1     # (Let x (Int 1))
  y = x + 3 # (Let y (Op + (Ref x) (Int 2)))
  y         # (Ref y)
  # factorial(start)
}

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