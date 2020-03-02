# const start = 5

fun main {
  # factorial(5)
}

# Factorial function
fun factorial(n int) int {
  if n {
    1
  } else {
    n * factorial(n - 1)
  }
}
