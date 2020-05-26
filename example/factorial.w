start = true
start = 3  # shadowing

fun main {
  factorial(start)
}

# Factorial function
fun factorial(n int) int {
  # y = 3
  # x, y, _ = 1, 2, 3
  # t = (1, 2, 3)
  # xs = for x in [1,2,3] { x * 2 }
  # if n <= 0 1 else n * factorial(n - 1)
  if n <= 0 {
    1
  } else {
    n * factorial(n - 1)
  }
}

# fun factorial(n float32) float32 {
#   if n <= 0.0 {
#     1.0
#   } else {
#     n * factorial(n - 1.0)
#   }
# }
