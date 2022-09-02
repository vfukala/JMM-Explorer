# Java Memory Model Explorer
This program (the Java Memory Model Explorer, JMMExplorer, or just the JMME) takes parts of the simplified source code of a multithreaded Java program as input. As output, it produces an extensive list of possible outputs of said Java program that are legal according to the JMM (Java Memory Model).

## Description
The JMME expects a list of files which contain snippets of Java source code. The source code in each file cannot contain any control flow structures -- each source code file is purely a sequence of statements including reads and loads of local/shared/volatile shared variables, arithmetic operations, and locking and unlocking operations on monitors.

## How to Build
required tools: `bison`, `flex`, `make` and a C++ compiler

0. If not using `g++` as the C++ compiler, change the first line of `makefile` accordingly.
1. In the root directory, run `./run_bison.sh`. (creates the `bin` directory to store all build products and runs `bison` to generate the parser)
2. In the root directory, run `make`. (builds the rest of the project as specified in the `makefile`)
3. The final JMME executable is located at `./bin/jmmexplorer`. (see How to Use below)

## How to Use
Create one or more source files. Let's say you created three source files called `source0`, `source1`, and `source2`. To simulate an execution of a Java program with each source file representing one thread, run the JMME (`./bin/jmmexplorer`) with each file as one argument. So in our example, `./bin/jmmexplorer source0 source1 source2`.

The JMME reads input only from the specified source files. It doesn't read standard input. If successful, it outputs the possible executions onto standard output. Otherwise, it uses standard output and standard error to print error messages.

## Output Format
If no errors occur during the processing of the source files (all source files exist, there are no syntax or compile semantic errors, etc.), the JMME outputs the possible executions in the following format:
* every possible output spans exactly one separate line
* every possible output is of one of two types: a division by zero exception or a list of printed values -- one value for each print statement in the source files
* in the case of an exception, the output of the JMME identifies the thread (by the name of the source file) and the line number where the exception occurred
* when there is no exception, the output of the JMME consists of multiple segments, one for each source file, separated by the pipe symbol (`|`); each segment consits of a list of integers separated and surrounded on each end by a space; there is one integer for each print statement in the source file and the integers are in the same order as the print statements appear in the source file

## Source Code File Format
The JMME supports a limited part of the Java language. Each source file is a sequence of semicolon-terminated statements. A statement is one of the following:
* a monitor lock statement or a monitor unlock statement
* an assignment statement
* a print statement
* an increment or a decrement statement

### Monitors
The source files can make use of an arbitrary number of monitor objects. Any two references to a monitor object are refer to the same monitor object if and only if they use the same name for the object (even if they are in different source files). A monitor object supports only two operations (method calls): `lock` and `unlock`. An lock statement has the form `{monitor name}.lock()` and an unlock statement has the form `{monitor name}.unlock()`. These statements provide the same semantics as the `lock` and `unlock` methods of Java's `Lock` class, that is, mutual exclusion and synchronization in the context of the JMM.

### Variables
The source files can make use of an arbitrary number of local, non-volatile shared, and volatile shared variables. All variables are of the Java `int` type (note that we don't consider the monitors to be variables in this context) and so is the value of every expression. All variables are automatically initalized to `0`. Any two references to a (volatile or non-volatile) shared variable refer to the same variable if and only if they use the same variable name (even if they are in different source files). References to a local variable refer to the same variable if and only they use the same variable name and they appear in the same source file. Every read/write access to a shared variable results in a read/write action as specified by the JMM. In comparison to non-volatile shared variables, accesses to volatile shared variables provides additional synchronization semantics as specified by the JMM.

### Expressions
Any reference to a variable can be an expression. An integer literal is an expression. An expression can be formed from other expressions using the following operators supported by the JMME
* binary `+` and `-`
* `*`, `/`, `%`
* unary `+` and `-`
* `&`, `^`, and `|`.

The semantics of these operators is as specified by the JLS (Java Language Specification).

### Assignment Statements
An assignment statements consists of a variable name followed by an assignment operator followed by an expression. An assignment operator is one of `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `^=`, `|=`. The semantics of the assignment is as specified by the JLS. Note, however, that in contrast to the JLS, only one assignment operator is allowed per statement. For example, `a = b = 42;` is legal Java code, but it is not allowed in the input source files of the JMME.

### Print Statements
For the sake of simplicity, each thread (source file) communicates output via calls to a function named `print`. A print statement is of the form `print(<expression>)`, where `<expression>` can be an arbitrary expression.

### Increment and Decrement Statements
Any variable can be incremented or decremented by one in an increment or a decrement statement. An increment statement is of the form `{variable name}++` and a decrement statement is of the form `{variable name}--`. The `++` and `--` may not appear outside of increment or decrement statements. (For example, `print(s++)` is not a legal statement.) Note that analogously to normal Java, `s++;` is semantically equivalent to `s += 1;`, which is semantically equivalent to reading the shared variable `s` into a local variable, adding one to the local variable, and writing the result back to the shared variable `s`. In particular, the increments (and similarly decrements) are not atomic operations.

### Identifiers
As in normal Java source code, the JMME uses identifiers to refer to variables and monitors. The first character of an identifier to used to determine the type it refers to.
* Identifiers starting with `m` refer to monitors.
* Identifiers starting with `l` refer to local variables.
* Identifiers starting with `s` refer to non-volatile shared variables.
* Identifiers starting with `v` refer to volatile shared variables.

Thanks to the above, there is no need to declare variables or monitors. A variable or a monitor 'exists' if it is accessed in any of the source files.

## Limitations
In theory, it is possible to determine _all_ the possible outputs that the JMM allows. In many cases, this is what the JMME does. However, there are cases where the JMM allows even more outputs than what the JMME can find. (Informally, the JMM allows executions where the dependency graph for the shared reads and writes contains cycles as long as these cycles can be 'optimized away'. The JMME only considers executions with formally acyclic dependency graphs.) Example 8 below illustrates this.

## Examples
All the examples below can also be found in the `examples` directory.
### Example 0
`source0` contains
```
print(-12 * 3 | 1);
print(9 ^ 3);
```
`source1` contains
```
print(6/2*(1+2));
```
When run on `source0` and `source1`, the JMME outputs
```
 -35 10 | 9 
```
(the expression inside each print statement is evaluated without any interaction with the rest of the program; there is only one possible output)
### Example 1
`source0` contains
```
print(s);
```
`source1` contains
```
s = 1;
```
When run on `source0` and `source1`, the JMME can output
```
 0 | 
 1 | 
```
because the read in the print statement in `source0` can see either the initializing write that writes `0` to `s` or the `s = 1` write in `source1`.
### Example2
`source0` and `source1` both contain
```
s++;
s++;
print(s);
```
When run on `source0` and `source1`, the JMME can output
```
 1 | 1 
 1 | 2 
 1 | 3 
 1 | 4 
 2 | 1 
 2 | 2 
 2 | 3 
 2 | 4 
 3 | 1 
 3 | 2 
 3 | 3 
 3 | 4 
 4 | 1 
 4 | 2 
 4 | 3 
 4 | 4 
```
(those are all the possible outputs according to the JMM when `s` is a non-volatile shared variable)
### Example 3
`source0` and `source1` both contain
```
v++;
v++;
print(v);
```
When run on `source0` and `source1`, the JMME can output
```
 1 | 2 
 2 | 1 
 2 | 2 
 2 | 3 
 2 | 4 
 3 | 2 
 3 | 3 
 3 | 4 
 4 | 2 
 4 | 3 
 4 | 4 
```
(those are all the possible outputs according to the JMM when `v` is a volatile shared variable)
### Example 4
`source0` and `source1` both contain
```
m.lock();
s++;
m.unlock();
m.lock();
s++;
m.unlock();
m.lock();
print(s);
m.unlock();
```
When run on `source0` and `source1`, the JMME can output
```
 2 | 4 
 3 | 4 
 4 | 2 
 4 | 3
 4 | 4 
```
(those are all the possible outputs according to the JMM)
### Example 5
`source0` contains
```
l = 32;
l_s = s;
l -= 7 * l_s * (l_s - 5);
print(l);
```
`source1` contains
```
l++;
l *= 2;
l *= l;
v = l | 3;
l *= l;
print(l);
```
`source2` contains
```
l = 3;
s = 5;
l %= 6;
l *= l + l * l;
print(l);
```
When run on `source0`, `source1`, and `source2`, the JMME outputs
```
 32 | 16 | 36 
```
because the `l` in every source file refers to a different local variable and the value of `l` doesn't depend on which writes in the other threads are seen (`7 * l_s * (l_s - 5)` in `source0` always evaluates to `0` because `0` and `5` are the only values that could've been read from `s` in `l_s = s;`)
### Example 6
`source0` contains
```
print(s * s);
```
`source1` contains
```
s = -1;
s = 1;
```
When run on `source0` and `source1`, the JMME can output
```
 -1 | 
 0 | 
 1 | 
```
because two read actions occur inside the print statement. Each of them can read `0`, `-1`, or `1` independently of the other one.
### Example 7
`source0` contains
```
print(v0 / v1);
```
`source1` contains
```
v0 += 563;
v1 += 7;
```
When run on `source0` and `source1`, the JMME can output
```
 0 | 
 80 | 
 division by zero exception in thread 0 (source0) at line 1
```
where the last output (with the exception) is returned in case that the read of `v1` in `source0` reads the value of `0` that `v1` is initially assigned.
### Example 8
`source0` contains
```
l1 = sx;
l2 = l1 | 1;
sy = l2;
print(l1);
print(l2);
```
`source1` contains
```
l3 = sy;
sx = l3;
print(l3);
```
When run on `source0` and `source1`, the JMME can output
```
 0 1 | 0 
 0 1 | 1 
```
which are both legal outputs according to the JMM. However, the JMM also allows the output ` 1 1 | 1 `, which the JMME doesn't discover.
