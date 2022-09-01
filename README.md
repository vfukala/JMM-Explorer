# Java Memory Model Explorer
This program (the Java Memory Model Explorer, JMMExplorer, or just the JMME) takes parts of the simplified source code of a multithreaded Java program as input. As output, it produces an extensive list of possible outputs of said Java program that are legal according to the Java Memory Model.

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
* a print statement
* an assignment statement
* an increment or a decrement statement
* a monitor lock statement or a monitor unlock statement

### Monitors
The source files can make use of an arbitrary number of monitor objects. Any two references to a monitor object are refer to the same monitor object if and only if they use the same name for the object (even if they are in different source files). A monitor object supports only two operations (method calls): `lock` and `unlock`. An lock statement has the form `{monitor name}.lock()` and an unlock statement has the form `{monitor name}.unlock()`. These statements provide the same semantics as the `lock` and `unlock` methods of Java's `Lock` class, that is, mutual exclusion and synchronization in the context of the JMM (the Java Memory Model).

### Variables
The source files can make use of an arbitrary number of local, non-volatile shared, and volatile shared variables. All variables are of the Java `int` type and so is the value of every expression. Any two references to a (volatile or non-volatile) shared variable refer to the same variable if and only if they use the same variable name (even if they are in different source files). References to a local variable refer to the same variable if and only they use the same variable name and they appear in the same source file. Every read/write access to a shared variable results in a read/write action as specified by the JMM. In comparison to non-volatile shared variables, accesses to volatile shared variables provide additional synchronization semantics as specified by the JMM.

### Identifiers
As in normal Java source code, the JMME uses identifiers to refer to variables and monitors. The first character of an identifier to used to determine the type it refers to.
* Identifiers starting with `m` refer to monitors.
* Identifiers starting with `s` refer to non-volatile shared integer variables.
