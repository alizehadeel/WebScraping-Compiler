# Workspace setup

Everything builds on Linux. On Windows, use **WSL 2 (Ubuntu)** and open the
repo from inside WSL in VS Code; native Windows builds of flex/bison/libxml2
are not supported by this project.

## 1. Tools

Ubuntu / WSL:

```bash
sudo apt update
sudo apt install -y build-essential flex bison libxml2-dev clang llvm gdb git
```

macOS (Homebrew): `brew install flex bison libxml2 llvm`, and put Homebrew's
bison and flex first on your `PATH` (the Apple ones are too old).

Check the versions:

```bash
bison --version   # needs 3.x (tested with 3.8.2)
flex --version    # tested with 2.6.4
g++ --version     # C++17
clang --version && llvm-as --version   # needed from milestone M3
xml2-config --version                  # needed once runtime/rt.c exists
```

## 2. VS Code

1. Windows only: install the **WSL** extension, then in a WSL terminal run
   `code .` from the repo folder.
2. Accept the recommended extensions when prompted (C/C++, Makefile Tools,
   Yash for `.l`/`.y` highlighting, LLVM IR highlighting).
3. `Ctrl+Shift+B` builds, the `test` task runs `make test`, and F5 debugs
   `dslc --ast` on the open `.dsl` file.

## 3. Build and test

```bash
make            # builds ./dslc
make test       # runs tests/run.sh (MODE=--ast until M2, then --run)
./dslc examples/isort.dsl --ast
```

`bison` runs with `-Werror -Wcounterexamples`, so a grammar conflict breaks
the build and prints a counterexample.

## 4. Git workflow

One branch per person, merged into `main` through a pull request that one
other member reviews.

```bash
git checkout -b <name>/<feature>
git push -u origin <name>/<feature>   # then open a PR on GitHub
```
