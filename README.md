
# ChainLang

ChainLang is a minimalistic, pipeline-centric programming language developed as a project for the Compiler Construction course. It is designed to demonstrate key compiler phases using a simple syntax that mimics dataflow pipelines, making it ideal for educational use.

## ✅ Functionalities Implemented

- Arithmetic operations (`+`, `-`, `*`, `/`)
- Variable declarations using `let`
- Conditionals (`if ... then ... else ... done`)
- Loops using `while ... done`
- Functions (`function ... end`)
- Exception handling with `try ... catch ... end`
- Pipeline operator (`->`) for chaining operations

## ⚙️ Technologies Used

- Flex for lexical analysis
- Bison for syntax parsing
- LLVM for IR generation and optimization
- C++ for backend integration

## 📄 Sample Program

```plaintext
let sum = 0 ->
let i = 1 ->

while i <= 10
    sum = sum + i ->
    i = i + 1
done ->

if sum > 50 && !(sum == 0) then
    avg = sum / 10
else
    avg = 0
done ->

output avg
```

## 🧾 LLVM IR Example Output

```llvm
define void @main() {
entry:
  %sum = alloca i32
  store i32 0, i32* %sum
  %i = alloca i32
  store i32 1, i32* %i
  br label %while

while:
  %i_val = load i32, i32* %i
  %cond = icmp sle i32 %i_val, 10
  br i1 %cond, label %loop_body, label %after

loop_body:
  %sum_val = load i32, i32* %sum
  %add = add i32 %sum_val, %i_val
  store i32 %add, i32* %sum
  %next_i = add i32 %i_val, 1
  store i32 %next_i, i32* %i
  br label %while

after:
  %sum_final = load i32, i32* %sum
  %cond1 = icmp sgt i32 %sum_final, 50
  %cond2 = icmp ne i32 %sum_final, 0
  %and = and i1 %cond1, %cond2
  br i1 %and, label %then, label %else

then:
  %avg = sdiv i32 %sum_final, 10
  br label %end

else:
  br label %end

end:
  ret void
}
```

## 🧠 Learning Objectives

- Understand compiler phases from lexing to code generation
- Use real tools (Flex, Bison, LLVM) in a practical compiler
- Explore custom syntax with enforced flow using `->`
- Get hands-on experience with function calls, conditionals, loops, and error handling

## 👨‍💻 Contributors

- Zain Mehdi (22L-6870)  
- Shehryar Ahmad (22L-6997)  
- Muhammad Zaid (22L-7001)

## 🔖 Notes

- `for` loop from the original proposal was replaced with a `while` loop for implementation clarity.
- All features mentioned above are implemented and tested.

---

> ChainLang simplifies complex compiler concepts into a clean, linear syntax that emphasizes data flow and program structure.
