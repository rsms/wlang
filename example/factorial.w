start = true
start = 3  # shadowing

fun main {
  factorial(start)
}

# Factorial function
fun factorial(n int) int {
  # y = 3
  # xs = for x in [1,2,3] { x * 2 }
  if n {
    1
  } else {
    n * factorial(n - 1)
  }
}
