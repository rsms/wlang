# const start = 5

# fun main {
#   factorial(5)
# }

# Factorial function
fun factorial(n, x int) int {
  if n == 0 {
    1
  } else {
    n * factorial(n - 1)
  }
}
