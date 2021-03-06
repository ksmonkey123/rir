# === modify env

f <- rir.compile(
  function(a) {
    localVar <- "local"
    a()
    localVar
})
rir.disassemble(f)

rir.compile(function()
    for (i in 1:400) f(function()1)
)()
rir.disassemble(f)

localVar <- 42
stopifnot(42 == f(function() rm("localVar", envir=sys.frame(-1))))

## === leak env

f <- rir.compile(
  function(a, b) {
    localVar <- "local"
    a()
    b
    localVar
})
rir.disassemble(f)

f(function()1, 2)
rir.disassemble(f)

rir.compile(function()
    for (i in 1:400) f(function()1, 2)
)()
rir.disassemble(f)

stopifnot(42 == f(function() leak <<- sys.frame(-1), assign("localVar", 42, leak)))
rir.disassemble(f)


{
    f <- function(x,y) x-y
    g <- rir.compile(function() f(44,2));
    h <- function() g();
    h();
    rir.markOptimize(g);
    h();
    stopifnot(h() == 42);
    rir.disassemble(g);
    h();
    f <- function(x,y) y-x;
    stopifnot(h() == -42);
    h()
}
