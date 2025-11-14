## 毕昇 C 简介
在系统编程领域，C/C++ 是应用最广泛的编程语言。在硬件资源十分受限的嵌入式场景下，C 语言使用的最多，但使用 C 语言编码存在很多痛点问题，比如 C 语言中指针使用带来的内存安全问题，C 语言缺乏原生的并发支持，以及一些基础的编程抽象(如泛型等)。近年来，在系统编程语言领域有不少探索的工作，比如 Rust，主打内存安全(所有权，生命周期，borrow checker 等)和并行并发(无栈协程)。Rust 是一门全新的编程语言，采用了和 C/C++ 完全不同的语言设计，学习曲线陡峭，也无法解决存量代码开发的问题。

在这个背景下，毕昇 C 采用了不同的策略，它基于 C 语言做了很多增强的设计，比如更强的内存安全特性，语言层面支持并发等，且可以在存量代码中渐进式的使用这些特性而不用完全重写已有代码。可以认为，毕昇 C 是 C 语言的一个超集。这本用户手册，将从以下三个方面介绍毕昇 C ：
- 基础编程抽象：成员函数，trait，泛型
- 内存安全：所有权，借用
- 并发：无栈协程

------

## 成员函数

### 概述
在 C 语言里，如果我们想表达某个类型的数据(data)有对应的某个方法(operation)，一般使用全局函数，让这个数据类型作为入参，如下：
```c
#include<stdio.h>
struct Data{
  int x;
};

void print_data(struct Data* data) {
    // 提供 print data 的实现逻辑
    printf("print data\n");
}

int main() {
    struct Data data = {.x = 1};
    print_data(&data);// expected result: print data
    return 0;
}
```

类似的，针对 int 类型我们可能需要 `print_int` 的函数，针对 float 类型需要 `print_float` 的函数，这不是一件令人愉快的事情，我们希望有一种更简洁的方式去表达类型与方法关联这件事，这就是为C语言引入成员函数的部分动机。引入成员函数后，上面例子的代码可以这么写：
```c
#include<stdio.h>
struct Data{
  int x;
};

void struct Data::print(struct Data* this) {
    // 提供 print data 的实现逻辑
    printf("print data\n");
}

void int::print(int* this) {
    // 提供 print int 的实现逻辑
    printf("print int\n");
}

void float::print(float* this) {
    // 提供 print float 的实现逻辑
    printf("print float\n");
}

int main() {
    struct Data data = {.x = 1};
    int a = 1;
    float b = 1.0;
    data.print();// expected result: print data
    a.print();// expected result: print int
    b.print();// expected result: print float
}

```

如果，我们想表达某些类型具有一组相似的行为，比如上面例子中的 `print` ，我们可以定义一个 `trait`，然后让 `struct Data` `int` `float` 等类型实现这个 `trait` 。成员函数和 `trait` 相结合，非常有表达力。关于 `trait` 的介绍，参考后续章节。

下面，我们简单介绍下毕昇 C 成员函数的一些具体规则：

### 基本语法

当我们想为某个类型增加一个成员函数时，我们只需要在普通函数定义的语法基础上，在函数名如 foo 前，增加 `typename::foo`，如下所示：
```c
void foo1() {
  // do nothing
};

void int::foo2(int* this) { // 实例成员函数，第一个入参是 This 指针，指向当前int类型的实例
  // do something
}

void int::foo3(int this) { // 实例成员函数，第一个入参是 This 实例，不是 This 指针
  // do something
}

void int::foo4() { // 静态成员函数
  // do something
}

```

其中，type-name 可以是基础类型如 `int`, `float` 等，也可以是用户自定义的结构体等，符合 C 语言对类型的定义规则，此外可以使用`This` 来简化表示当前类型。下面是更多的用法示例：

```c
// case 1
void int::print(int* this); // 声明

void int::print(int* this){ // 定义
    printf("int::print");
}

// case 2
struct S1{};
// 错误使用 S，在 C 语言里 struct S 才是一个类型
void S1::print(struct S1* this); // error: must use 'struct' tag to refer to type 'S'
void struct S1::print(struct S1* this); // Ok，修正后的声明

// case 3
typedef struct {
}S2;
void S2::print(S2* this); // Ok, S2 是 typedef 后的 struct S2

// case 4
void S2::print(This this); // Ok, This 表示当前类型 struct S2
void S2::print(This* this); // Ok, This* 表示指向当前类型 struct S2 的指针

```

采用这样的语法设计有一个好处，那就是我们很方便就可以给已有类型增加成员函数而不用侵入式修改源码。

### 关于 `this`

在上面的成员函数的例子中，如果函数参数中以 `this` 开头，则该函数为实例成员函数，并且此时 `this` 必须为第一个参数，它表示该成员函数对应类型实例(This this)、或指向该实例的指针(This* this)，它是一个“实例成员函数”。如果成员函数参数列表中，没有 `this` 存在，则表示这是一个“静态成员函数”。
```c
typedef struct {/*...*/} M;
void M::f(M* this, int i) {} // 实例成员函数

typedef struct {/*...*/} N;
void N::f() {} // 静态成员函数

int main() {
    M x;
    x.f(1); // Ok
    M::f(&x, 1); // Ok
    M* x1 = &x;
    x1->f(1); // Ok

    N y;
    y.f(); // Err: y does not have instance member function, use N::f instead.
    N::f(); // Ok
    return 0;
}

```
对于实例成员函数(第一个入参为`this`)，其调用方式有两种：

(1) 和访问成员变量类似，实例类型调用用 `.` 符号，如 `x.f(1)`；指针类型调用，用 `->` 符号，如 `x1->f(1)`。

(2) 普通函数调用方式，如 `M::f(&x, 1)`。

对于静态成员函数，其调用方式和调用普通全局函数类似，区别只是函数名变成 `type-name::func-name`，如 `N::f()`。

### 其他规则

- 成员函数支持声明和定义分开，如下：
```c
// 声明
const char* int::to_string(const int* this) ;

// 定义
const char* int::to_string(const int* this){
    // to_string 的实现，略
}

```
- 新增成员函数不影响原类型的 layout 包括 size 和 alignment
- 成员函数不支持重载和重定义
- 成员函数的名字不允许与成员变量相同，适用于 struct, union, enum 等
- 成员函数允许赋值给函数指针。
- 禁止对 cv-qualified type (被类型修饰符修饰的类型，如 const int 等) 添加成员函数
- 禁止对 “函数类型” "数组类型" "指针类型" 添加成员函数
- `this`指针允许被const/volatile等修饰符修饰
- 不允许给 incomplete type 扩展成员函数，incomplete type 即为那些声明了但未完整定义的类型
- 不允许给`void`类型扩展成员函数
- 如果两个头文件中对同一个类型扩展了同名的成员函数，那么在一个编译单元中包含这两个头文件，会导致编译错误
- 当前暂时禁止通过整数字面量、浮点数字面量、复合类型字面量直接调用成员函数

------

## 泛型

### 泛型概述

泛型是一种编程技术，允许将类型（如整数、字符串等基本类型，或者用户自定义的类型）作为参数传递给函数或结构体，从而实现代码复用、提升灵活性，并在保持类型安全的前提下，提高开发效率。

毕昇 C 的泛型是一种编译时的泛型机制，它可以定义一个通用的函数或类，然后根据不同的类型参数生成不同的实例。

目前，毕昇 C 已支持**泛型函数**、**泛型结构体**和**泛型类型别名**。

#### 实现动机

泛型的目的是为了提高代码的**效率**和**重用性**而实现的，其优点在于：

- 避免了代码冗余
- 提高了代码的可读性和可维护性
- 实现了类型安全和编译时检查
- ...

泛型编程使开发者能够编写适用于任意数据类型的通用算法，无需为不同类型（如整数、字符串或自定义类型）分别实现相同逻辑。

通过泛型，用户可以为函数或结构体定义通用模板，使得类中的某些数据成员或成员函数的参数、返回值取得任意类型。

#### 示例

以下方的代码为例：

```c
int sum_int(int a, int b) {
    int c = a + b;
    return c;
}

float sum_float(float a, float b) {
    float c = a + b;
    return c;
}

int main() {
    int sum1 = sum_int(1, 2);
    float sum2 = sum_float(1.2, 2.5);
}
```

可以看出，对于sum方法，如果要实现返回值分别为 **float** 和 **int** 两种情况，普通的 C 语言需要为同样实现的方法定义两次。

但是如果使用毕昇 C 的泛型语法，则只需定义一次，即可在实例化时重复使用，如下：

```c
#include<stdio.h>
T sum_t<T>(T a, T b) {
    T c = a + b;
    return c;
}

int main() {
    int sum1 = sum_t<int>(1, 2);
    float sum2 = sum_t<float>(1.2, 2.5);
    printf("%d\n",sum1); // expected result: 3
    printf("%f\n",sum2); // expected result: 3.700000
    return 0;
}
```

可以看出，泛型功能的引入，对于用了相同算法的声明场景，其代码量有明显的减少。

### 语法规则

对于毕昇 C 泛型，我们设计了如下的语法规则：

- 在声明泛型函数、结构体或类型别名时，泛型参数列表要用尖括号 '<>' 包裹起来，尖括号内部的类型一般是 identifier ，如 'T1', 'T2' 之类 。


- 在实例化时，既可以在尖括号内部传入具体的类型，如 int, float, struct S 等，也可以省略尖括号的书写，编译器会根据实际传入的参数进行隐式的类型推导。此外，如果将现有类型 typedef 为其他别名后，同样可以在实例化时用作泛型实参。


- 同时，毕昇 C 的泛型函数、泛型结构体和泛型类型别名也支持使用**常量参数**作为泛型的形参。

下面我们将分为五个部分详细说明。

#### 泛型函数

对于泛型函数，我们对语法规则的设计，主要体现在两个方面：

1. 声明时，区别于普通函数声明，我们需要在**函数名**和**函数入参**之间，添加一对尖括号 '<>' ，并在尖括号中写明泛型函数的**泛型形参**，其中形参可以为任何合法的名字（此处的合法指的是不会导致语义冲突的情况）；

   同时，对于函数返回值的类型，可以是普通的 builtin 类型，可以是用户已经定义过的结构体，也可以是泛型形参中的一个（如 T）。

2. 实例化时，类型既可以在尖括号中显式指定，也可以省略尖括号以及其中的内容，由编译器进行隐式推导：

   1. 显式指定类型时，区别于普通的函数调用，同样需要在被调用的**函数名**与**函数入参**之间添加一对尖括号 '<>' ，且中间无空格；然后在尖括号中传入**泛型实参**，此处的实参可以是 builtin 类型，也可以是用户已经定义过的结构体。
   2. 隐式推导时，写法与普通的函数调用相同，毕昇 C 编译器会根据函数调用传入的参数类型，自动推导**泛型实参**的类型。不过为了代码可读性等，推荐使显式指定类型。

下面是一些用法示例：

```c
typedef long int LT;

// 泛型函数
T max<T>(T a, T b) {						// 开头的'T'为函数返回值类型，而'<>'中的'T'泛型函数'max'的泛型形参
    T Max = a > b ? a : b;
    return Max;
}

int main() {
    int a = 3;
    int b = 5;
    int c = 4;

    // 泛型函数 实例化
    int max1 = max<int>(a, b);				// 显示指定泛型实参类型
    int max3 = max(a, c);				    // 隐式推导，编译器自动推导'T'为 int 类型

	return 0;
}
```



#### 泛型结构体

对于泛型结构体，我们对语法规则的设计，同样体现在两个方面：

1. 声明时，区别于普通结构体声明，我们需要在**结构体名**的后面之间，添加一对尖括号 '<>' ，并在尖括号中写明泛型结构体的**泛型形参**。
2. 实例化时，泛型结构体仅支持显式指定类型，即：显示指定类型时，区别于普通的结构体的构造，同样需要在被构造的**结构体名**后面添加一对尖括号 '<>' ，且中间无空格；然后在尖括号中传入**泛型实参**，此处的实参可以是 builtin 类型，也可以是用户已经定义过的结构体。
3. 在使用泛型结构体类型时，如声明泛型结构体类型的变量、声明泛型结构体类型的成员、声明参数、声明返回值、扩展成员函数时，可以省略 struct。即在使用形如`struct S<T>`的类型时，可以简写为`S<T>`，但在声明该类型时不允许省略。

下面是一些用法示例：

```c
typedef long int LT;

// 泛型struct
struct S<T, B>{
    T a;
    B b;
};

// 泛型union
union MyUnion<T1, T2> {
    T1 u1;
    T2 u2;
};

// 返回类型为'泛型union'的泛型函数'foo_union'
union MyUnion<T1, T2> foo_union<T1, T2>(union MyUnion<T1, T2> *this) {
    return *this;
}

int main() {
    // 泛型struct 实例化
    struct S<int, LT> s1 = {.a = 42, .b = 5};		// 使用 typedef 后的类型作为泛型实参

    // 泛型union 实例化
    union MyUnion<int, float> p;
    foo_union(&p);		// 泛型函数 foo_union 的隐式类型推导

    return 0;
}
```

#### 泛型成员函数

在支持定义泛型 struct 和泛型 union 类型的基础上，还可以为它们扩展普通成员函数。

如下是一个用法示例，该示例包含了声明泛型成员函数、定义泛型成员函数及实例化调用泛型成员函数：

```c
struct MyStruct<T> {
    T res;
};

union MyUnion<T> {
    T res;
};

T struct MyStruct<T>::foo(struct MyStruct<T>* this, T a) { // 定义泛型成员函数
    this->res = this->res + a;
    return this->res;
}

T union MyUnion<T>::foo(union MyUnion<T>* this, T a) {
    this->res = this->res + a;
    return this->res;
}

int main() {
    struct MyStruct<int> s = { .res = 1 };
    union MyUnion<int> u = { .res = 5 };
    int res1 = s.foo(2); // 调用泛型成员函数
    int res2 = u.foo(6);
    return 0;
}
```

关于泛型成员函数，还有以下几点需要注意：

1. 泛型成员函数可以是静态函数，即第一个参数可以不为`this`；
2. 不允许泛型成员函数的重载，这与成员函数的规则相同；

```
   
   struct MyStruct<T> {
       T val;
   };
   
   T static struct MyStruct<T>::static_add(T a, T b) {
       return a + b;
   }
   
   
   T struct MyStruct<T>::static_add(T a);  // expected result: error: conflicting types for 'static_add'
   
   
   int main() {
       int sum = MyStruct<int>::static_add(10, 20);   // 静态泛型成员函数
       return 0;
   }
```


3. 泛型成员函数的返回类型可以是泛型 struct 或泛型 union类型。

```
   
   struct MyStruct<T> {
       T val;
   };
    
   struct MyStruct<T> struct MyStruct<T>::make_struct(T x) {
       struct MyStruct<T> result = { .val = x };
       return result;
   }
   
   
   int main() {
       MyStruct<int> s = MyStruct<int>::make_struct(42);
       return 0;
   }
```

#### 常量泛型

除了基础泛型的实现，毕昇 C 还引入了常量泛型的功能。具体来说，常量泛型是一种允许程序项在常量值上泛型化的特性。也就是说，常量可以作为泛型参数传递到泛型变量中，代码会根据常量参数而进行特化，从而确保无开销，并可以直接在代码中作为常量来使用。

例如，在 毕昇 C  中，可以定义一个泛型结构体，其中一个泛型参数是一个**常量泛型参数**，该参数可以用于表示结构体内定义的数组的大小。

这样，通过在实例化时传入不同的常量值，便可以生成多个不同大小的数组对象，如：

```c
struct Array<T, int N> {
    T data[N];
};

int main() {
    struct Array<int, 5> arr1;
    struct Array<int, 10> arr2;
    return 0;
}
```

如上，这里的 '10' 和 '5' 就是常量泛型参数 'int N' 的实参，它们决定了数组的大小。

目前，泛型常量的规则如下：

- 常量泛型的形参只支持“可编译时计算的类型”，目前只支持整数类型
- 常量泛型的实参只能是“编译时可计算”的常量表达式
- 语法上，如果只是 int 字面量、constexpr 常量，那么可以不需要小括号，其它常量表达式一律需要用小括号

目前，毕昇 C 对于常量泛型的实现仅限于整形的“整数常量”：

- 对于声明时，形参列表仅接受 int 及其修饰符 long、short、unsigned、signed，以及上述关键字的各种组合。同时也支持将上述关键字 typedef 成其他别名再做为形参。
- 对于实例化时，目前泛型实参列表支持 IntegerLiteral （即常量1，2之类），同时也支持 constexpr修饰的变量与常量表达式。

下面是一些用法示例：

```c
#include <stdio.h>

typedef long long int LLInt;

// 使用了泛型常量的泛型函数
int print_dataSize<T, int B>()
{
    T data[B];
    printf("the size of data is %d\n", B);
  	return B;
}

// 使用了 typedef别名 作为泛型常量的泛型函数
void print_const<LLInt B>() {
    printf("the const is %d\n", B);
}

// 使用了泛型常量的泛型struct
struct Array_1<T, int N>
{
  	T data[N];
};

// 使用了 typedef别名 作为泛型常量的泛型struct
struct Array_2<LLInt B, int C, T>
{
  	LLInt data1[B];
  	int data2[C];
  	T a;
};

int main() {
  	int a1 = print_dataSize<int, 5>();
  	print_const<20>();

  	struct Array_1<int, 5> arr1;
  	struct Array_2<5, 6, int> arr2 = {.a = 1};

    return 0;
}
```

#### 泛型类型别名
标准 C 已有类型别名的功能，语法为：
```c
typedef OldType NewType;
```
为了使类型别名能够与泛型一起配合使用，BSC 引入与标准 C 语法不同的类型别名语法：
```c
typedef NewType = OldType ;
```
加上泛型参数就形成了泛型类型别名，例如：
```c
typedef MyPointerType<T> = T*;
```
泛型类型别名的使用可以简化类型的书写，还可以为类型赋予更具描述性的名称，使代码更易于阅读和理解，举例来说：

我们可以使用 HashMap 来记录某年级学生的各科成绩，Key 表示学生的学号，Value 表示学生的各科成绩，根据实际情况的不同，学生的学号可以用 int、string 等类型表示，成绩可以用 int、float 等类型表示，科目数量也可能发生变化，使用泛型类型别名有助于我们根据实际需求进行定制：

```C
//辅助类及成员方法
struct HashMap<K, V> {
   //省略实现
};
void struct HashMap<K, V>::insert(This* this, K key, V value) {
   //省略实现
}
struct Array<T, int N> {
    T a[N];
};
//使用泛型类型别名定义一个特殊的HashMap类型，它的key是T1类型，value是一个长度为N，元素类型为T2的数组类型：
typedef GenericGrade<T1, T2, int N> = struct HashMap<T1, struct Array<T2, N>>;
//继续使用类型别名，针对不同的年级进行定制：
typedef Grade1 = GenericGrade<int, int, 3>;  //一年级学生学号为int，成绩为int，科目数量为3
typedef Grade3 = GenericGrade<int, float, 4>;//三年级学生学号为int，成绩为float，科目数量为4
typedef Grade6 = GenericGrade<int, float, 5>;//六年级学生学号为int，成绩为float，科目数量为5

int main() {
    Grade1 grade1;
    grade1.insert(10, {80, 90, 95});
    grade1.insert(11, {80, 95, 90});
    Grade3 grade3;
    grade3.insert(12, {90.0, 95.5, 90.0, 85.0});
    grade3.insert(13, {80.0, 90.0, 95.5, 90.0});
    Grade6 grade6;
    grade6.insert(15, {80.0, 90.0, 95.5, 90.0, 85.0});
    grade6.insert(16, {80.0, 90.0, 95.5, 90.0, 85.0});
    return 0;
}
```

对于类型别名，有以下规则：
1. 不允许为泛型类型别名扩展成员函数。
```C
struct S<T> {};
typedef MyS<T> = struct S<T>;

void MyS<T>::foo(This* this) {} //error: extended type of a BSC member function cannot be a generic typealias
```
2. 不允许在结构体类型内部定义类型别名。
```C
struct S<T> {
    typedef type = T;    //error: type name does not allow storage class to be specified
    typedef Int64 = int; //error: expected ';' at end of declaration list
};
```
3. 不允许在函数体内定义泛型类型别名，但可以定义普通类型别名。
```C
void foo<T>() {
    typedef type = T;
    typedef Int64 = int;

    typedef MyType1<T> = T;   //error: no template named 'MyType1'
    typedef MyType2<T1> = T1; //error: BSC generic typealias cannot be declared within a function
}
```

下面是一些用法示例：
```C
struct S<T> {};
struct V<T1, T2> {
    T1 a;
    T2 b;
};

// 普通类型别名
typedef Int64 = long int;   //等价于 typedef long int Int64;
typedef MyS = struct S<int>;//等价于 typedef struct S<int> MyS;

// 泛型类型别名
typedef MyPointerType<T> = T*;
typedef Array_3<T> = T[3];
typedef Array_N<T, int N> = T[N]; //支持带常量泛型参数

typedef MyS_T_T<T> = struct V<T, T>;
typedef MyS_T_int<T> = struct V<T, int>;

int main() {
    Int64 a = 5;  //等价于int a = 5;
    MyS s;        //等价于struct S<int> s;
    int b = 2;
    MyPointerType<int> c = &b;  //等价于int* c = &b;
    Array_3<int> d = {1,2,3};   //等价于int d[3] = {1,2,3};
    Array_N<int, 3> e= {1,2,3}; //等价于int e[3] = {1,2,3};
    MyS_T_T<int> s2;
    MyS_T_int<int> s3;
    return 0;
}
```

##### conditional 泛型类型别名
在 bsc_conditional.hbs 这一 BSC 标准库中提供了 conditional 泛型类型别名，可以实现类型层面的“分支逻辑”：
```c
// bsc_conditional.hbs
typedef conditional<int C, T, F> = __conditional(int C, T, F);
```
当 C 非 0 时，conditional 类型别名指代类型 T ，否则指代类型 F，条件表达式必须是可以编译期求值的常量表达式：
```c
#include<bsc_conditional.hbs>  //使用conditional需要导入头文件
int main() {
    conditional<1, int, double> a = 1;   //等价于int a = 1;
    conditional<0, int, double> b = 1.0; //等价于double b = 1.0;
    return 0;
}
```
使用conditional，不仅可以简化书写，可以在编译时根据条件选择不同的类型，避免了运行时的条件分支，提高代码的效率，以下是一个关于选择函数返回类型的使用案例：

定义一个泛型函数，它的返回值类型取决于泛型参数 T ，如果 T 是指针类型，则返回值类型仍然是 T ，否则，返回 T 的指针类型。

C++ 中需要借助泛型特化和 concept 来实现：
```cpp
// T 是指针类型时，std::is_pointer_v<T> == true，匹配该版本:
template<typename T> requires std::is_pointer_v<T>
T foo() { ... }

// T 不是指针类型时，匹配该版本:
template<typename T>
T* foo() { ... }
```

在 BSC 中，我们可以借助 conditional 很方便地实现这样的功能，相比C++更加方便易用：
```c
typedef PointerType<T> = conditional<is_pointer<T>(), T, T*>;

PointerType<T> foo<T>() { ... }
```



------

## 内存安全

毕昇 C 内存管理的目标是将常见的时间类内存安全问题，如悬挂引用/内存泄漏/重复释放堆内存/解引用空指针等常见的内存安全问题在编译阶段暴露出来。

为此，毕昇 C 引入了[所有权](###所有权)和[借用](###借用)两个新的概念，所有权用关键字`owned`实现，借用通过关键字`borrow`表示，并通过`safe`和`unsafe`两个关键字来约束所有权和借用的执行范围。

### 安全区

#### 概述：

c 语言有很多规则过于灵活，不方便编译器做静态检查。因此我们引入一个新语法，使得在一定范围内的毕昇 c 代码必须遵循更严格的约束，保证在这个范围内的代码肯定不会出现“内存安全”问题。

允许用`safe`和`unsafe`关键字修饰函数、语句、括号表达式。

- `unsafe`表示这段代码在非安全区，这部分代码遵循标准 c 的规定，同时这部分代码的安全性由用户保证。


- `safe`表示这段代码在安全区，这部分代码必须遵循更严格的约束，同时编译器可以保证内存安全。


- 没有 `safe`或`unsafe`关键字修饰的全局函数默认是非安全的。


#### 代码示例：

```c
#include <stdlib.h>

typedef struct File {
} FileID;

// 无关键字修饰，表示按默认非安全区
FileID *owned create(void) {
  FileID *p = malloc(sizeof(FileID));
  return (FileID * owned) p;
}
FileID *owned consume_and_return(FileID *owned p) { return p; }

// 使用 safe 修饰函数，表示该函数为安全函数，函数内为安全区
safe void file_safe_free(FileID *owned p) {
  // 使用 unsafe 修饰代码块，表示这段代码在非安全区。这段代码是在安全区内的非安全区，也属于非安全区
  unsafe { free((FileID *)p); }
}

int main(void) {
  FileID *owned p1 = create();
  FileID *owned p2 = consume_and_return(p1);
  // 使用 safe 修饰语句，表示这段代码在安全区
  safe file_safe_free(p2);
  return 0;
}
```

#### 语法规则：

1. 允许使用`safe/unsafe`修饰函数声明、函数签名、函数定义、函数指针、语句、括号表达式。

```c
// 修饰函数签名
safe int test(int, int);
// 修饰函数声明
safe int test(int a, int b);
// 修饰函数定义
safe int test(int a, int b) { return a + b; }
// 修饰函数指针
safe int (*p)(int, int);

safe int main(void) {
  // 修饰代码块
  safe {
    int a = 1;
  }
  unsafe { int b = 1; }
  // 修饰语句
  safe int c = 1;
  unsafe c++;
  // 修饰括号表达式
  char d = unsafe((char)c);
  return 0;
}
```

2. 不允许使用`safe/unsafe`修饰全局变量、函数外类型声明、`typedef`声明（允许修饰函数指针)。

```c
// error: 不允许修饰全局变量
safe int g_a;
// error: 不允许修饰函数外类型声明
safe struct b { int a; };
// error: 不允许修饰 typedef
safe typedef int mm;

int main() { return 0; }
```

3. `safe`修饰的函数，参数类型和返回类型必须是`safe`类型。

   非`safe`类型包括：裸指针类型、`union`类型、成员中包含不安全类型的`struct`类型、成员中包含不安全类型的数组类型。

```c
// error: 返回值为非安全类型的裸指针类型
safe int *test1(int a);
// error: 参数类型为非安全类型的裸指针类型
safe int test2(int *a);

typedef struct F {
  int *a;
} SF;
// error: 返回值为成员中包含不安全裸指针类型的 struct 类型
safe SF test3(int a);
// error: 参数类型为成员中包含不安全裸指针类型的 struct 类型
safe int test4(SF b);
```

4. `safe`修饰的函数，函数参数列表不可以省略。`safe void test(); `是不允许的， `safe void test(void); `是允许的。

5. `safe`修饰的函数，函数参数列表不可以包含变长参数。`safe int test(int a,  ...); `是不允许的。

6. 如果`trait`中的函数被声明为`safe`，那么要求实现`trait`的类型的对应成员函数也必须是`safe`修饰的函数。若`trait`中的函数未声明为`safe`，也允许实现`trait`中的类型的成员函数为`safe`，但编译器会给出**warning**。

```c
trait G {
  safe int *owned test1(This * owned this);
  int *owned test2(This * owned this);
};
// ok: trait 实现函数必须为 safe
safe int *owned int ::test1(int *owned this) { return this; }
// 非 safe 函数的实现可以为 safe
safe int *owned int ::test2(int *owned this) { return this; }
impl trait G for int;

int main() { return 0; }
```

7. 多个同名函数声明必须有同样的`safe/unsafe`修饰。

```c
safe int test(int a);
// error: 多个函数声明中 safe/unsafe 不一致
unsafe int test(int a);
```

8. 多个同名函数声明排除`safe/unsafe`修饰后，应是完全一致的。

```c
safe int test(int a);
// error: 函数声明不完全一致
safe int test(int a, int b);
```

9. `safe`修饰泛型函数时，会对泛型每个实例化版本也做`safe`检查。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe T F<T>(T a) { return a; }

void test() {
  int a = 1;
  int b = F<int>(a);
  int *owned c = (int *owned)safe_malloc(1);
  int *owned d = F<int * owned>(c);
  // error: 实列化函数入参和返回值为非安全的裸指针类型
  int *e = F<void *>((void *)0);
  safe_free((void *owned)d);
}

int main() {
  test();
  return 0;
}
```

10. 成员函数也可以被`safe/unsafe`修饰，其规则和全局函数一样。

```c
struct MyStruct<T> {
  T res;
};
safe T struct MyStruct<T>::foo_a(T a) {
  return a;
}

int main() { return 0; }
```

11. 对于`safe`修饰的函数指针类型，与赋值的函数参数和返回值类型必须是一致的。

```c
safe void test1(int a) {}
safe void test2(void) {}
void test3(void) {}
safe void (*p)(int a);
int main() {
  p = test1;
  // error: 参数类型不一致，不允许赋值
  p = test2;
  // error: 参数类型不一致，不允许赋值
  p = test3;
  return 0;
}
```

12. 安全区内被调用的函数或函数指针必须是`safe`的函数签名，不允许调用非安全函数或函数指针。

```c
safe void test1(void) {}
unsafe void test2(void) {}
safe void (*test3)(void);
unsafe void (*test4)(void);
int main() {
  safe {
    test1();
    // error: 安全区内不允许调用非安全函数
    test2();
    test3();
    // error: 安全区内不允许调用非安全函数指针
    test4();
  }
  unsafe {
    test1();
    test2();
    test3();
    test4();
  }
}
```

13. 安全区内允许再包含`unsafe`修饰的语句、函数指针、括号表达式，非安全区内也允许再包含`safe`修饰的语句、函数指针、括号表达式。

```c
int test1(int a, int b) { return a + b; }
safe int test2(int a, int b) { return a > b ? a : b; }
int main() {
  safe {
    int a = 0;
    unsafe a++;
    unsafe {
      a = test1(1, 3);
      safe a = test2(3, 5);
    }
  }
}
```

14. 安全区内不允许无初始化或初始化不完整的变量声明。

```c
struct S {
  int age;
  char name[20];
};
void test() {
  safe {
    // error: 安全区内不允许无初始化的变量声明
    int a;
    // error：安全区不允许部分初始化
    struct S tom = {10};
    struct S tony = {10, "tony"};
  }
}

int main() {
  test();
  return 0;
}
```

15. 安全区内`switch`语句中的`case/default`只能存在于`switch`后面的第一层代码块中，且第一层代码块不允许有变量定义。

```c
safe void test(int a) {
  switch (a) {
    // error: 第一层代码块不允许有变量定义
    int b = 10;
    case 0: {
        int c = 1;
        break;
    }
    {
        // error: case 只能存在于 switch 后面的第一层代码块中
        case 1 : { break; }
    }
    {
        // error: default 只能存在于 switch 后面的第一层代码块中
        default: { break; }
    }
  }
}

int main() {
  int a = 1;
  test(a);
  return 0;
}
```

16. 安全区内不允许使用自增 （++）、自减（--）操作符，不允许使用`union`，不允许裸指针通过`->`访问成员，

    允许owned指针和borrow指针通过`->`访问成员。

```c
union un {
  int age;
  char name[16];
};
struct F {
  int age;
};

void test(void) {
  struct F d = {10};
  struct F *e = &d;
  struct F *owned f = (struct F * owned) & d;
  struct F *borrow i = &mut d;
  safe {
    int a = 1;
    // error: 安全区不允许自增
    a++;
    // error: 安全区不允许自减
    a--;
    // ok: 安全区允许 += 运算符
    a += 1;
    // ok: 安全区允许 -= 运算符
    a -= 1;
    // error: 安全区不允许使用 union
    union un b = {10};
    // error: 安全区不允许 union 通过“.”访问成员
    int c = b.age;
    // error: 安全区不允许裸指针通过“->”访问成员
    int g = e->age;
    // ok: 允许owned指针“->”通过访问成员
    int h = f->age;
    // ok: 允许borrow指针“->”通过访问成员
    int j = i->age;
  }
}

int main() {
  test();
  return 0;
}
```

17. 安全区内不允许使用取地址符`&`（允许对函数取地址），只允许`&const`，`&mut`取借用。

    安全区内不允许解引用裸指针类型，但可以解引用`owend`指针类型和`borrow`指针类型。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

void test() {
  safe {
    int a = 10;
    // error: 安全区不允许取地址符号
    int *b = &a;
    // error: 安全区不允许解引用裸指针
    int c = *b;
    int *owned d = safe_malloc(2);
    // ok: 允许解引用owned指针
    int e = *d;
    safe_free((void *owned)d);
    int *borrow f = &mut a;
    // ok: 允许解引用borrow指针
    int g = *f;
  }
}

int main() {
  test();
  return 0;
}
```

18. 安全区内不允许指向类型不同的指针类型之间转换，但允许其他类型owned指针显式转换为void类型的owned指针。

    安全区内不允许指针和非指针类型之间的转换，不允许`owned/borrow/raw`指针之间的转换。

```c
void test() {
  int *pa;
  double *pb;
  safe {
    // error：不允许指向类型不同的指针类型之间转换
    pb = pa;
    // error：不允许指向类型不同的指针类型之间转换
    pa = pb;
    // error：不允许指向类型不同的指针类型之间转换
    pb = (double *)pa;
  }
  int i;
  safe {
    // error：不允许指针和非指针类型之间的转换
    pa = i;
    // error：不允许指针和非指针类型之间的转换
    i = pa;
  }
  int *owned pd = (int *owned)pa;
  safe {
    // error：不允许 owned/raw 指针之间的转换
    pa = pd;
    // error：不允许 owned/raw 指针之间的转换
    pd = pa;
    // ok : 允许显式转换为void类型的owned指针
    void *owned pe = (void *owned)pd;
  }
}

int main() {
  test();
  return 0;
}
```

19. 安全区内不允许表达范围从大向小的类型转换（比如从`long`转换为`int`，从`int`转换为`_Bool`，从`int`转换为`enum`）。不允许表达精度从高向低的类型转换（比如从`double`转换为`float`）。对于基础类型的常量发生类型转换，如果目标类型可以描述这个值，那么该类型转换是允许的，并且在`if/while`中也允许转换。

```c
void test() {
  long a;
  int b;
  _Bool c;
  double e;
  float f;
  safe {
    // ok: 允许表达范围从小向大的类型转换
    a = b;
    // error: 不允许表达范围从大向小的类型转换
    b = a;
    a = 1;
    // _Bool可以描述1
    c = 1;
    // error: 目标类型不可以描述这个值，不允许转换
    c = 2;
    e = f;
    // error：不允许表达精度从高向低的类型转换
    f = e;
    f = 1.0;
    f = 1.2f;
  }
}

int main() {
  int a = 2;
  safe {
    // ok: 在 if 中允许转换
    if (a) {
      a += 1;
    } else {
      a -= 1;
    }
  }
  test();
  return 0;
}
```

20. 安全区内不允许内嵌汇编语句。

```c
void test() {
  safe {
    int ret = 0;
    int src = 1;
    // error: 安全区不允许内嵌汇编
    asm("move %0, %1\n\t" : "=r"(ret) : "r"(src));
  }
}
int main() { return 0; }
```

### 所有权

#### 0. 前言

C 语言作为一种系统级编程语言，
提供了对指针的高度灵活的直接操作以及利用内存管理函数使开发者手动精细控制和管理内存的能力，
因而被广泛地应用于各种需要直接与硬件或内存等系统资源交互的领域和场景。
然而，这种内存管理模式存在容易导致内存泄漏、释放后使用、空指针解引用、缓冲区溢出和越界读写等内存安全问题。
内存安全问题不仅会造成资源的浪费，也可能导致程序行为错误，甚至导致程序崩溃，对程序的稳定性造成威胁。
内存安全问题可以划分为**时间内存安全**和**空间内存安全**两大类，其中时间内存安全包含内存泄漏、释放后使用、空指针解引用等，空间内存安全包括缓冲区溢出、越界读写等。
BiShengC 语言的内存管理为解决程序的时间内存安全问题，利用了**所有权特性**在编译期对潜在的内存安全问题进行检查，识别潜在的时间内存安全错误。


#### 1. 特性简介

BiShengC 语言的所有权特性被用于确保程序中的指针及其指向内存空间能被正确地管理。
在 BiShengC 语言中，使用`owned`关键字用来修饰一个指针类型，表明该指针拥有其指向的内存的所有权。
拥有所有权的指针必须确保其指向的内存在指针作用域结束前被显式释放，否则存在潜在的内存泄漏错误；
此外，一块堆内存只能同时被一个`owned`指针所拥有，`owned`指针为移动语义，这样避免了释放后使用等内存安全问题的发生。
以下是一段使用了所有权特性的 BiShengC 语言代码，用于了解所有权特性：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int *owned takes_and_gives_back(int *owned p) { return p; }

safe void test(void) {
  // 通过提供的 safe_malloc 申请一块大小为 sizeof(int) 的堆内存，并将值设置为2
  int *owned p = safe_malloc(2);
  // 将 p 指向的堆内存转移给 q，后续不可再使用 p 访问这块内存，否则编译报错
  int *owned q = p;
  unsafe {
    // 通过函数参数转移走 q 的所有权，但通过函数返回值归还所有权
    q = takes_and_gives_back(q);
    // 在 q 的作用域结束前调用 safe_free
    // 安全释放堆内存，此处不释放则会报内存泄漏错误
    safe_free((void *owned)q);
  }
  return;
}

int main() {
  test();
  return 0;
}
```

在安全区，`owned`指针指向的内存一定为堆上内存（如通过`safe_malloc`函数申请出的堆内存），`owned`指针不可能指向栈内存，在作用域结束前必须转移所有权或释放（如`safe_free`函数进行释放）。

这两个函数的函数原型如下：
```c
T *owned safe_malloc<T>(T t);
void safe_free(void *owned);
```
可以通过`safe_malloc`申请足够存放`T`类型的堆空间，并初始化为`t`，当`owned`指针生命周期结束前，必须通过`safe_free`进行释放，在使用时，需要将`T *owned`指针转换为`void *owned`指针类型再释放，对于**多级指针，需要从内到外进行释放**；对于结构体内部有`owned`指针成员的情况，需要**先将结构体内全部`owned`指针成员释放后，才能释放结构体的`owned`指针**。

函数使用示例：
```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *owned p;
  int *owned q;
};

safe void test(void) {
  //变量所有权初始化
  int *owned pi = safe_malloc(1);
  // 结构体所有权初始化
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *owned s1 = safe_malloc(s);
  // 多级指针所有权初始化
  int *owned p = safe_malloc(1);
  int *owned *owned pp = safe_malloc(p);
  // 变量所有权释放
  safe_free((void *owned)pi);
  // 结构体所有权释放(从内到外)
  safe_free((void *owned)s1->p);
  safe_free((void *owned)s1->q);
  safe_free((void *owned)s1);
  // 多级指针所有权释放(从内到外)
  safe_free((void *owned) * pp);
  safe_free((void *owned)pp);
}

int main() {
  test();
  return 0;
}
```

#### 2. 语法及语义规则

为实现所有权特性，BiShengC 语言引入了`owned`关键字用于修饰指针类型的变量。
该关键字与`const`、`restrict`以及`volatile`均属于类型修饰符，其语法如下：

```
type-qualifier:
  const | restrict | volatile | ownership-qualifier

ownership-qualifier:
  owned
```

具体而言，所有权特性的语法及部分语义规则有以下几点：

1. `owned`关键字仅允许在 BiShengC 语言编译单元内使用；

2. `owned`关键字仅被允许用于修饰指针类型，不允许修饰非指针类型，修饰多级指针时，每级指针的类型修饰可以不一样，规则与`const`类似；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe int main(void) {
  int *owned p = safe_malloc(2);
  // error: owned 关键字必须修饰指针
  int owned a = 2;
  double *owned q = safe_malloc(1.1);
  // error: owned 关键字必须修饰指针
  double owned b = 1.1;
  // ok: 可以修饰多级指针
  int *owned *owned pp = safe_malloc(p);
  double *d = (double *)malloc(sizeof(double));
  double **owned pd = safe_malloc(d);
  // error: 多级指针释放时，应当释放从内向外释放
  safe_free((void *owned) * pp);
  safe_free((void *owned)pp);
  free(*pd); // 也可直接 free(d);
  safe_free((void *owned)pd);
  safe_free((void *owned)q);
  return 0;
}
```

3. 允许使用`owned`关键字修饰结构体指针及结构体的指针成员；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int m;
  int n;
};

struct R {
  int *owned p;
  double *owned q;
};

safe void test(void) {
  struct S s = {.m = 1, .n = 2};
  struct S *owned sp = safe_malloc(s);
  // error: 要用 owned 不允许修饰结构体变量
  struct S owned so = {.m = 1, .n = 2};
  struct R r = {.p = safe_malloc(1), .q = safe_malloc(2.5)};
  struct R *owned rp = safe_malloc(r);
  safe_free((void *owned)sp);
  // 先释放结构体内的 owned 指针成员
  safe_free((void *owned)rp->p);
  safe_free((void *owned)rp->q);
  // 再释放结构体 owned 指针
  safe_free((void *owned)rp);
}

int main() {
  test();
  return 0;
}
```

4. `owned`不允许修饰`union`类型和`union`的成员，且`union`的每个成员均不能拥有`owned`修饰的成员；

```c
struct S {
  int a;
  int b;
};

struct T {
  int *owned p;
  struct S s;
};

union A {
  int a;
  // error: 禁止 union 成员用 owned 修饰
  int *owned p;
  struct S s;
  // error: 禁止 union 成员用 owned 修饰
  struct S *owned sp;
  // error: 禁止 union 成员结构体有 owned 成员
  struct T t;
  // error: 禁止 union 成员用 owned 修饰
  struct T *owned tp;
  // ok: 不跟踪裸指针指向的变量的 owned 成员
  struct T *trp;
};
```

5. `owned`修饰的类型或拥有`owned`修饰的成员的类型不可以作为数组的成员；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct A {
  int *owned p;
};

safe void test(void) {
  // error: 数组成员不能被 owned 修饰
  int *owned arr_i[2] = {safe_malloc(1), safe_malloc(2)};
  // error: 数组成员不能被 owned 修饰
  struct A arr_a[2] = {{safe_malloc(1)}, {safe_malloc(2)}};
}
```

6. `owned`修饰的指针不支持算术运算符（指针偏移操作），但支持比较运算符；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe int main(void) {
  int *owned p = safe_malloc(2);
  int *owned q = safe_malloc(3);
  // error: owned 指针不支持算术运算符
  p += 1;
  // error: owned 指针不支持 [] 运算符
  p[3] = 3;
  // error: owned 指针支持比较运算符
  if (p == q) {
  }
  unsafe {
    safe_free((void *owned)p);
    safe_free((void *owned)q);
  }
  return 0;
}
```

7. `owned`修饰的类型与非`owned`修饰的类型之间不允许隐式类型转换；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe int main(void) {
  unsafe {
    int *b = (int *)malloc(sizeof(int));
    // error: owned 指针不允许隐式类型转换
    int *owned p = b;
  }
  int *owned q = safe_malloc(3);
  // error: owned 指针不允许隐式类型转换
  int *c = q;
  safe_free((void *owned)q);
  return 0;
}
```

8. 基本类型一致时，允许`owned`指针类型与非`owned`指针类型之间的显式强制类型转换，且这种转换只能在非安全区进行；
基本类型不一致时，只允许`void * owned`类型与`T * owned`类型之间的相互强制转换；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int main() {
  int *i = (int *)malloc(sizeof(int));
  // ok: i 和 pi 的基本类型均为 int
  int *owned pi = (int *owned)i;
  double *d = (double *)malloc(sizeof(double));
  // error: d 和 pd 基本类型不一致
  int *owned pd = (int *owned)d;
  double *owned dd = safe_malloc(1.5);
  // ok: 允许显式转换为 void *owned
  void *owned pdd = (void *owned)dd;
  safe_free((void *owned)pi);
  safe_free((void *owned)d);
  safe_free((void *owned)pdd);
  return 0;
}
```

9. `owned`允许修饰指向`trait`的指针，假设有一个具体类型`S`，它实现了`trait T`，则：

    - `S * owned`类型可以隐式转换为`trait T * owned`类型；
    - `trait T * owned`类型允许被显式转换为`void * owned`类型。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

trait T{};

impl trait T for int;

void test() {
  int *owned pi = safe_malloc(1);
  // ok: 隐式转换为 trait T *owned
  trait T *owned pti = pi;
  // ok: 显式转换为 void *owned
  void *owned pvi = (void *owned)pti;
  safe_free(pvi);
}

int main() {
  test();
  return 0;
}
```

10. 通过函数指针调用函数的时候，规则与一般的函数调用一样，会在函数调用时检查形参类型与实参类型是否匹配及返回类型与返回值类型是否匹配；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int add(int *a, int *b) { retun *a + *b; }
typedef int (*FTP)(int *, int *);
typedef int (*FTOP)(int *owned, int *);

void test() {
  FTP ftp = add;
  int *owned pa = safe_malloc(1);
  int *owned pb = safe_malloc(2);
  // error: 类型不匹配
  ftp(pa, pb);
  // error: 类型不匹配
  FTOP ftop = add;
  safe_free((void *owned)pa);
  safe_free((void *owned)pb);
}

int main() {
  test();
  return 0;
}
```

11. `owned`可以修饰 trait 类型，即`trait T* owned`，也表示该变量拥有其内部存储的数据的所有权。
    该类型可以作为类型声明、函数的入参类型及函数的返回值类型。但当前不支持`trait T* owned`与`trait T*`之间的类型转换。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

trait T { safe void release(This * owned this); };
struct IPv4 {
  char *buf1;
};
struct IPv6 {
  char *buf1;
  char *buf2;
};
safe void struct IPv4::release(struct IPv4 *owned this) {
  unsafe { free(this->buf1); }
  safe_free((void *owned)this);
}
safe void struct IPv6::release(struct IPv6 *owned this) {
  unsafe {
    free(this->buf1);
    free(this->buf2);
  }
  safe_free((void *owned)this);
}
impl trait T for struct IPv4;
impl trait T for struct IPv6;

void cleanup(trait T *owned t) { t->release(); }

int main() {
  struct IPv4 ipv4 = {.buf1 = "192.168.1.1"};
  struct IPv6 ipv6 = {.buf1 = "2001:0db8:85a3:0000",
                      .buf2 = "0000:8a2e:0370:7334"};
  struct IPv4 *owned sipv4 = safe_malloc(ipv4);
  struct IPv6 *owned sipv6 = safe_malloc(ipv6);
  trait T *owned tipv4 = sipv4;
  trait T *owned tipv6 = sipv6;
  // 使用 trait T* owned 作为入参
  tipv4->release();
  tipv6->release();
  safe_free((void *owned)tipv4);
  safe_free((void *owned)tipv6);
  return 0;
}
```

#### 3. 所有权状态转移规则

在对所有权特性的语法和部分语义有了解后，本节将对所有权的状态转移规则进行详细阐述。
为更好地理解所有权特性对内存安全带来的保障，首先需要明白程序执行时的堆栈内存模型。

总体而言，程序执行时内存可分为栈区和堆区两个部分，这两部分内存一起为程序在运行时提供内存空间。
栈区即为调用栈，保存的是程序执行所需要维护的所有信息。
每当一次函数调用发生时，就会创建一个对应的栈帧，函数调用的上下文、函数的入参以及函数体内的局部变量就存在这个栈帧中。
栈帧的基址一般由 rbp 寄存器指向，而栈顶由 rsp 寄存器指向，两个寄存器共同标识了一个函数的栈帧。
当一次函数调用结束时，相应的栈帧就会在函数返回前被销毁，相应的内存空间也就得到释放。
这个过程是通过调整 rbp 寄存器的值为调用者的栈帧基址、rsp 的值为调用者的栈顶地址完成的，这也是为什么栈区的变量不需要显式释放。
对于堆区，则存放的是那些在运行时动态分配内存的数据。
一个典型的例子是对于`int *p = malloc(sizeof(int))`这种操作，需要由操作系统在堆区找到一块适合大小的内存空间用于分配，然后将这块内存的地址存在 p 中，指针 p 是程序的一个栈上变量。
虽然堆区的内存分配更为灵活，但却缺乏组织，不正确的内存管理很容易导致堆区的内存泄漏。
例如，当一次函数调用完成后，其局部变量 p 被销毁，而其指向的堆内存未被显式地调用`free(p)`进行回收，则这块堆内存将永远无法被回收，产生了内存安全错误。

<div align="center">
    <img src=../BiShengCLanguageUserManualImages/memory-leak-example.png width=60% />
</div>

利用 BiShengC 语言提供的所有权特性，可以用`owned`关键字对那些需要管理的指针进行标识，这样就可以在程序编译时检查出潜在的错误，避免在运行时出现错误。
以下是 BiShengC 语言所有权的核心规则：

1. 在 BiShengC 语言中每一个值都被一个`owned`指针变量所拥有，该`owned`指针变量即为值的所有者；
2. 一个值同时只能被一个`owned`指针变量所拥有，即一个值只能拥有一个所有者；
3. 当`owned`指针变量离开作用域范围时，需要释放其拥有的值所在的堆内存。

基于以上核心规则，接下来结合详细的代码示例进行具体介绍。

##### 3.1 转移所有权

**1. 一个拥有所有权的变量`s1`被赋值给另一个变量`s2`是一个移动语义，该操作后变量`s1`失去了对值的所有权，原先的变量`s1`无法再使用。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe void test(void) {
  int *owned p = safe_malloc(10);
  int *owned q = p;
  // error: p 的所有权已移交给q了
  int *owned m = p;
  unsafe {
    safe_free((void *owned)q);
    safe_free((void *owned)m);
  }
}

int main() {
  test();
  return 0;
}
```

在这个例子中，`p`拥有一块堆内存的所有权，这块内存大小为`sizeof(int)`，存储的值为10。
在声明`q`时，将`p`的所有权转移给了`q`，`p`不再拥有对这块堆内存的所有权。
则在声明`m`时，便不可再将`p`的所有权转移给`m`，编译器会在此处报错。
因此，利用所有权特性，可以保证一个值只能拥有一个所有者。
（那么如果没有这条规则会出现什么后果呢？三个指针会同时指向一块内存，在作用域结束时对这块内存释放三次，出现重复释放的错误）

**2. 一个拥有所有权的变量`s1`作为整体被赋值给另一个变量`s2`时，如果`s1`内部还有其他拥有所有权的指针，则会全部都转移给`s2`。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *p;
  int *owned q;
};

void test(void) {
  struct S s = {.p = (int *)malloc(sizeof(int)), .q = safe_malloc(1)};
  struct S *owned s1 = safe_malloc(s);
  struct S *owned s2 = s1;
  // error: q 的所有权被一并交给 s2 了
  int *owned p = s1->q;
  safe_free((void *owned)s2->q);
  safe_free((void *owned)s2);
  safe_free((void *owned)p);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，将`s1`指向堆内存的所有权转移给了`s2`，但同时`s1`内部还有一个拥有所有权的指针`s1->q`，因此也会一并将他的所有权转移给`s2->q`，后续再使用`s1->q`时便会报错。

**3. 一个拥有所有权的变量`s1`作为整体被赋值给另一个变量`s2`时，如果`s1`内部还有其他拥有所有权的指针，则必须保证内部其他`owned`指针均拥有所有权，才能将`s1`赋值给`s2`。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *owned p;
  int *owned q;
};

safe void test(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *owned s1 = safe_malloc(s);
  int *owned p = s1->p;
  // error: s1 已无对全部成员的所有权
  struct S *owned s2 = s1;
  safe_free((void *owned)p);
  safe_free((void *owned)s2->p);
  safe_free((void *owned)s2->q);
  safe_free((void *owned)s2);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，我们先将`s1->p`的所有权转移走，然后试图整体转移`s1`的所有权给`s2`，但此时`s1->p`已经不再持有对任何一块堆内存的所有权，因此这个操作是不合法的。

**4. 一个拥有所有权的变量`s1`在失去其所有权后，可以通过赋值的方式使其再次拥有指向某块堆内存的所有权，这样就可以再次使用`s1`。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe void test(void) {
  int *owned p = safe_malloc(10);
  // 移走 p
  int *owned q = p;
  // 重新拿到新元素的所有权
  p = safe_malloc(4);
  // 仍然可以将所有权交给其他指针
  int *owned m = p;
  safe_free((void *owned)q);
  safe_free((void *owned)m);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，`p`的所有权转移给`q`后，再次调用了`safe_malloc`函数为其重新赋予了一块堆内存的所有权，因此后续仍可将`p`的所有权转移给`m`。

**5. 不允许将所有权转移给一个已经拥有所有权的变量。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

safe void test(void) {
  int *owned p = safe_malloc(12);
  int *owned q = safe_malloc(67);
  // error: p 有拥有其他指针的所有权
  q = p;
  safe_free((void *owned)p);
  safe_free((void *owned)q);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，试图将`p`的所有权转移给`q`，但`q`此时已经拥有所有权，再试图转移的话会使`q`原先指向的堆内存泄漏，因此无法进行转移，会在编译时报错。

**6. 如果一个变量`s1`拥有所有权，而其内部的`owned`指针变量的所有权已被转移，如果想再次赋予内部变量所有权，需要保证内部变量的所有父`owned`指针变量均拥有所有权。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *owned p;
  int *owned q;
};

safe void test(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *owned s1 = safe_malloc(s);
  struct S *owned s2 = s1;
  // error: s1 已不再拥有所有权，无法给内部成员赋予所有权
  s1->p = safe_malloc(5);
  safe_free((void *owned)s2->p);
  safe_free((void *owned)s2->q);
  safe_free((void *owned)s2);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，`s1`、`s1->p`以及`s1->q`的所有权均被转移给了`s2`，后续再试图赋予`s1->p`所有权时，其父`owned`变量指针`s1`尚未拥有所有权，因此此次操作是非法的，会在编译时报错。

##### 3.2 作用域结束时的内存释放

**1. 对于所有的`owned`指针变量，会在其词法作用域结束时检查其是否依然拥有堆内存的所有权，如果依然拥有，则存在内存泄漏错误。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *owned p;
  int *owned q;
};

safe void test(void) {
  int *owned p = safe_malloc(2);
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *owned s1 = safe_malloc(s);
  struct S *owned s2 = s1;
  // error: 未释放 s2 内部成员所有权及 s2 所有权；未释放 p 所有权
  // safe_free((void *owned)p);
  // safe_free((void *owned)s2->p);
  // safe_free((void *owned)s2->q);
  // safe_free((void *owned)s2);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，当作用域结束时，编译器会发现`p`、`s2`、`s2->p`以及`s2->q`依然拥有其指向的堆内存的所有权，即这些堆内存都没有被释放，因此会编译失败并报告内存泄漏。

##### 3.3 强制类型转换

**1. 允许将`T * owned`类型的变量通过强制类型转换转为`void * owned`类型，但转换成功的条件为变量依然拥有所有权且其内部的`owned`指针变量均已不拥有所有权。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *owned p;
  int *owned q;
};

safe void test(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *owned s1 = safe_malloc(s);
  int *owned p = s1->p;
  unsafe {
    // error: s1 仍然拥有内部成员(q)所有权，无法转为 void *owned
    safe_free((void *owned)s1);
    safe_free((void *owned)p);
  }
}

int main() {
  test();
  return 0;
}
```

在这个例子中，试图将`s1`强制类型转换为`void * owned`类型，但`s1->q`依然拥有所有权，因此转换失败。

##### 3.4 函数调用与返回

**1. 函数调用和返回时，如果函数的形参或函数的返回值为`owned`指针类型，则要求传入的实参以及返回值必须拥有堆内存的所有权。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *owned p;
  int *owned q;
};

struct S *owned F(struct S s) {
  struct S *owned ret = safe_malloc(s);
  return ret;
}

void test(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  int *owned p = s.p;
  // error: s 已不再拥有全部内部成员的所有权
  struct S *owned s1 = F(s);
  safe_free((void *owned)p);
  safe_free((void *owned)s1->p);
  safe_free((void *owned)s1->q);
  safe_free((void *owned)s1);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，传入`F`函数的结构体变量`s`内部有两个`owned`指针变量，而`s.p`已经被转移走，因此这次函数调用是非法的，会编译报错。

#### 4. 源源变换

BiShengC 语言的 clang 编译器支持源源变换功能，即将`.cbs`文件转换为等价的`.c`文件。
所有权特性仅引入了`owned`关键字表示所有权，在源源变换时只会去掉所有的`owned`关键字，然后生成相应的`.c`代码。
关于源源变换的详细细节，请参考手册的源源变换章节。


### 借用
借用作为毕昇C内存安全特性的重要组成部分，是对所有权的一个补充。前面小节描述了所有权特性(ownership)，对某个资源拥有所有权的主体，有责任释放这个资源。这个小节，将介绍对资源的借用(borrow)。

#### 1. 特性简介
如果我们只有 ownership 类型，由于函数调用、赋值等操作都会转移所有权，那么代码能力会非常受限。在编程时，我们常常需要表达“对某个资源进行借用”的概念，区别于“拥有某个资源”。正如现实生活中，如果一个人拥有某样东西，你可以从他那里借来，当使用完毕后，也必须要物归原主。

#### 1.1 借用的定义及借用操作符
毕昇 C 的**借用是一个指针类型，指向了被借用对象存储的内存地址**。为表达借用的概念：
1. 引入新关键字`borrow`，用`borrow`来修饰指针类型 T*，表示 T 的借用类型
2. 引入借用操作符 `&mut` 和 `&const`，其中，`&mut e`表示获取对表达式 e 的**可变借用**，`&const e`表示获取对表达式`e`的**只读借用**。此处要求表达式`e`是 lvalue（左值）与标准 C 中的取地址操作符`&`类似, 借用操作符实际上获取的是表达式`e`的地址

例如，我们可以创建指向局部变量`local`的可变借用`p1`和不可变借用`p2`，并使用它们：
```C
void use_immut(const int *borrow p) {}
void use_mut(int *borrow p) {}

void test() {
  int local = 5;
  // p1是local的可变借用指针
  int *borrow p1 = &mut local;
  use_mut(p1);
  // p2是local的不可变借用指针
  const int *borrow p2 = &const local;
  use_immut(p2);
}

int main() {
  test();
  return 0;
}
```
另外，表达式`e`如果是指针的解引用表达式，`&mut *p`和`&const *p`分别可以看作对地址`p`中存放的值，也就是`*p`，取可变借用和不可变借用，**这一操作不为`*p`产生临时变量**。其中，`p`可以是裸指针、`owned`指针和其它借用指针。例如：
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

void test() {
  int *x1 = malloc(sizeof(int));
  *x1 = 1;
  // p1借用了*x1
  int *borrow p1 = &mut * x1;
  int *owned x2 = safe_malloc(2);
  // p2借用了*x2
  int *borrow p2 = &mut * x2;
  int local = 3;
  int *borrow x3 = &mut local;
  // p3借用了*x3
  int *borrow p3 = &mut * x3;
  safe_free((void *owned)x2);
}

int main() {
  test();
  return 0;
}
```
#### 1.2 借用的作用
假设我们有这样的一个需求：创建一个文件，并且调用一些操作函数对文件进行读写操作。如果没有借用的概念，调用文件操作函数会导致文件指针所有权的转移，为了使文件指针在函数调用之后仍然可以被使用，我们需要再将所有权返回给调用方：
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

typedef struct {
  int file_id;
} MyFile;

MyFile *owned create_file(int id) {
  MyFile f = {.file_id = id};
  return safe_malloc(f);
}
void file_safe_free(MyFile *owned p) { safe_free((void *owned)p); }

MyFile *owned insert_str(MyFile *owned p, char *str) {
  // some operation to insert a string to file
  printf("%s to file %d\n", str, p->file_id);
  // 通过返回值，将所有权转移给调用方，避免所有权转移
  return p;
}

MyFile *owned other_operation(MyFile *owned p) {
  // some operation
  // 通过返回值，将所有权转移给调用方，避免所有权转移
  return p;
}

int main() {
  MyFile *owned p = create_file(0);
  char str[] = "insert str";
  // p的所有权先被移动到 insert_str 中，再通过返回值转移到调用方
  p = insert_str(p, str);
  p = other_operation(p);
  file_safe_free(p);
  return 0;
}
```
这种写法会造成文件指针所有权的频繁转移，在代码逻辑较为复杂的时候很容易出错，而且如果所有权被转移走了但是没有归还，后续将无法再使用该文件指针。有了借用，将对文件指针的借用作为参数传递给操作函数，函数返回之后文件指针仍可以用于后续其他操作，不再需要像上面那个例子一样，先通过函数参数传入所有权，然后再通过函数返回来传出所有权，代码更加简洁：
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

typedef struct {
  int file_id;
} MyFile;

MyFile *owned create_file(int id) {
  MyFile f = {.file_id = id};
  return safe_malloc(f);
}
void file_safe_free(MyFile *owned p) { safe_free((void *owned)p); }

void insert_str(MyFile *borrow p, char *str) {
  // some operation to insert a string to file
  printf("%s to file %d\n", str, p->file_id);
  // 无需返回所有权
}

void other_operation(MyFile *borrow p) {
  // some operation
  // 无需返回所有权
}

int main() {
  MyFile *owned p = create_file(0);
  char str[] = "insert str";
  // 所有权不会被移动
  insert_str(&mut * p, str);
  // 所有权不会被移动
  other_operation(&mut * p);
  file_safe_free(p);
  return 0;
}
```

#### 2.借用变量和被借用对象的生命周期
##### 2.1 生命周期及其作用
我们可以对不同种类的对象取借用：`owned`变量、非`owned`类型的局部变量、全局变量、临时匿名变量、参数等，甚至是某个复合变量的一部分。为正确表示借用变量和不同种类被借用对象的有效作用域，我们引入生命周期的概念。

生命周期检查的主要作用是避免悬垂指针，它会导致程序使用本不该使用的数据，以下 C 代码就是一个使用了悬垂指针的典型例子：

```C
int main() {
  int *p;
  {
    int local = 5;
    p = &local;
  }
  *p = 1;
  return 0;
}
```
这段 C 代码有两点值得注意：
1. `int *p`的声明方式存在使用`NULL`的风险；
2. `p`指向了内部block中的`local` 变量，但是`local`会在block结束的时候被释放，因此回到外部block后，`p`会指向一个无效的地址，是一个悬垂指针，它指向了提前被释放的变量`local`，可以预料到，`*p = 1` 会导致该段程序运行时出现未定义行为（undefined behavior）。当代码逻辑较为复杂时，这类异常行为很难被发现。

对于第一点，毕昇 C 规定：和裸指针不同，表示借用的`borrow`指针必须被初始化，表示对某一个具体对象的借用；

对于第二点，毕昇 C 规定：**任何一个对资源的借用，都不能比资源的所有者的生命周期长**。也就是说：借用变量的生命周期，不能比被借用对象的生命周期长。

接下来我们使用毕昇 C 的借用特性改写上面的 C 代码，通过检查借用变量和被借用对象的生命周期，在编译期就可以识别出潜在的内存安全风险：
```C
int main() {
  int local1 = 1;
  // 借用指针变量 p 必须被初始化，否则会报错
  int *borrow p = &mut local1;
  {
    int local2 = 2;
    // 对 p 进行再赋值之后，p 不再借用 local1，而是借用 local12
    p = &mut local2;
  }
  // error: local2 的生命周期不够长
  *p = 3;
  return 0;
}
```

##### 2.2 借用变量和被借用对象
每个借用变量（也就是 borrow 指针变量）都会有一个或多个被借用对象，例如：
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int g = 5;
void test(int a, int *owned b, int *c, struct S d) {
  // 被借用对象是普通局部变量
  int local = 5;
  int *borrow p1 = &mut local; // p1的被借用对象是local
  int *borrow p2 = &mut * p1;  // p2的被借用对象是*p1
  int *borrow p3 = p1;         // p3的被借用对象是*p1

  // 被借用对象是owned变量
  int *owned x1 = safe_malloc<int>(2);
  int *borrow p4 = &mut * x1; // p4的被借用对象是*x1

  // 被借用对象是裸指针变量
  int *x2 = malloc(sizeof(int));
  int *borrow p5 = &mut * x2; // p5的被借用对象是*x2

  // 被借用对象是结构体的某个字段
  struct S s = {.a = 5};
  int *borrow p6 = &mut s.a; // p6的被借用对象是s.a

  // 被借用对象是函数的返回值，与被调用函数的借用类型入参的 “被借用对象” 一样
  int local1 = 10, local2 = 20;
  // 被调用函数call有两个借用类型入参，因此p7的被借用对象是local1和local2
  int *borrow p7 = call(&mut local1, &mut local2);

  // 被借用对象是全局变量
  int *borrow p8 = &const g; // p8的被借用对象是g

  // 被借用对象是函数入参
  int *borrow p9 = &mut a;    // p9的被借用对象是a
  int *borrow p10 = &mut * b; // p10的被借用对象是*b
  int *borrow p11 = &mut * c; // p11的被借用对象是*c
  int *borrow p12 = &mut d.a; // p12的被借用对象是d.a
}

int main() {
  test();
  return 0;
}
```

##### 2.3 借用变量的 Non-Lexical Lifetime
一个变量的生命周期从它的声明开始，到当前整个语句块结束，这个设计被称为Lexical Lifetime，因为变量的生命周期是严格和词法中的作用域范围绑定的。这个策略实现起来非常简单，但它可能过于保守了，某些情况下借用变量的作用范围被过度拉长了，以至于某些实质上是安全的代码也被阻止了，这在一定程度上限制了程序员能编写出的代码。因此，毕昇 C 为借用变量引入 Non-Lexical Lifetime（简写为NLL），用更精细的手段计算借用变量真正起作用的范围，**借用变量的 NLL 范围为：从借用处开始，一直持续到最后一次使用的地方**。具体的，它是**从借用变量定义或被再赋值开始，到被再赋值之前最后一次被使用结束**。

其中，以下场景属于对借用变量p的使用：
1. 函数调用，如`use(p)`或`use(&mut *p)`
2. 函数返回`return p`或`return &mut *p`
3. 解引用`*p`
4. 成员访问`p->field`

举例来说：
```C
void use(int *borrow p) {}
void other_op() {}

// 本例中p的NLL是分段的，每段NLL都有一个被借用对象
void test() {
  int local1 = 1, local2 = 2;  //#1
  int *borrow p = &mut local1; //#2，p的第一段NLL开始，被借用对象为local1
  other_op();                  //#3
  use(p);                      //#4，p的第一段NLL结束
  other_op();                  //#5
  p = &mut local2;             //#6，p的第二段NLL开始，被借用对象为local2，由于后面没有再对p的使用，p的NLL结束
  other_op();     //#7
}
// p的NLL是：[2,4]->local1, [6,6]->local2

int main() {
  test();
  return 0;
}
```
##### 2.4 被借用对象的 Lexical Lifetime
与借用变量不同，被借用对象的生命周期是 Lexical Lifetime，对于不同种类被借用对象的生命周期，我们给出具体的定义：
| 被借用对象种类 | | 生命周期定义 |
| ---- | ---- | ---- |
| 全局变量 | | 全局变量的生命周期是整个程序，从程序开始到退出，一直存在 |
| 局部变量 | owned变量 | 从变量定义开始，到它被 move 走结束（owned struct类型如果没有被move，生命周期会在当前block结束的时候结束）
| | 非owned非borrow变量 | 从变量定义开始，到当前 block 结束 |
| 局部字面量 | "string literal" | 从使用处开始，到当前 block 结束的时候结束 |
| | (struct S) { ... } | 从使用处开始，到当前 block 结束的时候结束
| e->field | | e 的生命周期 |
| e.field | | e 的生命周期 |
| e[index] | | e 的生命周期 |
| *e | | e 的生命周期 |

#### 2.5 借用的生命周期约束
在2.1中我们提到过，对于借用，我们有这样的生命周期约束：**借用变量的生命周期，不能比被借用对象的生命周期长**。
举例来说：
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

void use(int *borrow p) {}
int *borrow call(int *borrow p, int *borrow q) { return p; }

// 本例中，p的生命周期为[2,4]，被借用对象local的生命周期为[1,4]，满足生命周期约束
void test1() {
  int local = 5;              //#1
  int *borrow p = &mut local; //#2
  use(p);                     //#3
} //#4

// 本例中，p的生命周期有两段“
// ok: 第一段为[2,2]，被借用对象local的生命周期为[1,8]，满足生命周期约束
// error: 二段为[5,7]，被借用对象local1的生命周期为[4,6]，不满足生命周期约束
void test2() {
  int local1 = 5;              //#1
  int *borrow p = &mut local1; //#2
  {                            //#3
    int local2 = 5;            //#4
    p = &mut local2;           //#5
  }                            //#6
  use(p);                      //#7
} //#8

// 本例中p的生命周期有两段：
// ok: 第一段为[2,2]，被借用对象local的生命周期为[1, 8]，满足生命周期约束
// error: 第二段为[5,7]，被借用对象有两个，分别是local和local1，其中local1的生命周期为[4, 6]，不满足生命周期约束
void test3() {
  int local1 = 5;                       //#1
  int *borrow p = &mut local1;          //#2
  {                                     //#3
    int local2 = 5;                     //#4
    p = call(&mut local1, &mut local2); //#5
  }                                     //#6
  use(p);                               //#7
} //#8

// 本例中，if分支对p进行了重新赋值，在#10
// error: use(p)时，p的被借用对象local2的生命周期已经结束，不满足生命周期约束
// ok" else分支满足生命周期约束
void test4() {
  int local = 5;              //#1
  int *borrow p = &mut local; //#2
  int local1 = 5;             //#3
  if (rand()) {               //#4
    int local2 = 5;           //#5
    p = &mut local2;          //#6
  } else {                    //#7
    p = &mut local1;          //#8
  }                           //#9
  use(p);                     //#10
}

//本例中，p的生命周期为[2,4]，被借用对象*x的生命周期为[1,3]，不满足生命周期约束，error
void test5() {
  int *owned x = safe_malloc<int>(5); //#1
  int *borrow p = &mut * x;           //#2
  safe_free((void *owned)x);          //#3
  use(p);                             //#4
} //#5

int main() {
  test1();
  test2();
  test3();
  test4();
  test5();
  return 0;
}
```

#### 3.可变借用和不可变借用
毕昇 C 将借用指针的权限进行了分级，分为可变（mut）借用和不可变（immut）借用，我们可以通过操纵可变借用指针来读写被借用对象的内容，通过不可变借用指针，我们只能读取被借用对象的内容，但是不能修改它。例如：
```C
// 可变借用指针类型为 T *borrow
void use_mut(int *borrow p) {
  // 通过可变借用指针，可以修改被借用对象的值
  *p = 5;
  // 通过可变借用指针，可以读取被借用对象的值
  int a = *p;
}

// 不可变借用指针类型为 const T *borrow
void use_immut(const int *borrow p) {
  // error: 无法通过不可变借用指针，来修改被借用对象的值
  *p = 5;
  // 通过不可变借用指针，可以读取被借用对象的值
  int a = *p;
}

int main() {
  int i = 1;
  int *borrow pmi = &mut i;
  use_mut(pmi);
  const int *borrow pimi = &const i;
  use_immut(pimi);
  return 0;
}
```
##### 3.1 `&mut e`要求 e 是可修改的
我们在 1.1 中提到过，`&mut e`和`&const e`要求表达式 e 是 lvalue，即 e 是可以被取地址的，对于可变借用表达式`&mut e`，我们还要求 e 是可变的，具体的：
| lvalue表达式 | 是否可修改 |
| ---- | ---- |
| ident | 变量 ident 没有被 const 修饰，且ident 不能是函数名 |
| "string literal" | 不允许，因为字符串常量保存在常量区，不能写 |
| (struct S) { ... } | 允许 |
| e->field | 要求 e 是可变借用指针，或者是指向可修改类型的 owned 指针，或者是指向可修改类型的裸指针，且field没有被const修饰，多级 field 的情况应该要求每一级的 field 都没有 const 修饰 |
| e.field | 要求 e 是可变的，且field没有被const修饰，多级 field 的情况应该要求每一级的 field 都没有 const 修饰 |
| e[index] | 要求 e 是可变的 |
| *e | 要求 e 是可变借用，或者是指向可修改变量的裸指针 |

##### 3.2 可变借用同时只能存在一个
如果有两个或更多的指针同时访问同一数据，并且至少有一个指针被用来写入数据，可能会导致未定义行为，例如：
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

void free_a(int *a) { free(a); }
void read_a(int *a) { printf("%d\n", *a); }

void test() {
  int *a = malloc(sizeof(int));
  *a = 42;
  int *p1 = a;
  int *p2 = a;
  // 该函数会释放 a 所指向的内存
  free_a(p1);
  // 该函数会读取 a 所指向的内存
  read_a(p2); // 打印一个脏值
}

int main() {
  test();
  return 0;
}
```
由于借用本质上也是指针，所以为了避免上述问题，毕昇 C 规定，**同一时刻，对于同一个对象，要么只能拥有一个可变借用, 要么任意多个不可变借用**。
```C
void write(int *borrow p) {}
void read(const int *borrow p) {}

void test1() {
  int local = 1;
  int *borrow p1 = &mut local;
  // error: 同一时刻最多只能有一个指向local的可变借用变量
  int *borrow p2 = &mut local;
  write(p1);
  write(p2);
}

void test2() {
  int local = 1;
  int *borrow p1 = &mut local;
  // error: 指向local的可变和不可变借用不能同时存在
  const int *borrow p2 = &const local;
  write(p1);
  read(p2);
}

void test3() {
  int local = 1;
  const int *borrow p1 = &const local;
  // error: 指向local的可变和不可变借用不能同时存在
  int *borrow p2 = &mut local;
  read(p1);
  write(p2);
}

int main() {
  test1();
  test2();
  test3();
  return 0;
}
```

如果同时存在对一个变量的可变借用和不可变借用，可能会出现通过可变借用修改被借用对象的内存状态，然后再使用不可借用访问被修改的内存，从而导致未定义行为的情况。
例如：

```C
#include <stdio.h>

struct A {
  int *p;
};

const int *borrow struct A::get_p(This *borrow this) {
  return &const * (this->p);
}

void struct A::free_p(This *borrow this) { free(this->p); }

int main() {
  struct A a = {.p = malloc(sizeof(int))};
  // q借用了a.p
  const int *borrow q = a.get_p();
  // a.p指向的内存被释放
  a.free_p();
  // *q的操作可能会导致未定义行为
  printf("%d\n", *q);
  return 0;
}
```

上述代码中，`a.free_p()`实际上使用了一个指向 a 的可变借用，该可变借用会使在它之前被定义的借用 q 失效，由于`printf("%d\n", *q)`使用了失效的 q，毕昇 C 编译器会报错，也就阻止了不安全行为的发生。

由于不可变借用不会导致被借用对象被修改，因此同一时刻可以拥有任意多个不可变借用，例如：
```C
void read(const int *borrow p) {}

void test() {
  int local = 5;
  const int *borrow p1 = &const local;
  const int *borrow p2 = &const local;
  read(p1);
  read(p2);
}

int main() {
  test();
  return 0;
}
```

#### 4.借用对被借用对象的影响
##### 4.1 不可变借用对被借用对象的影响
对表达式 e 做不可变借用， 即`&const e`，在这个不可变借用的生命周期结束之前，e 只能读不能修改，也不能对 e 创建可变借用。
| 不可变借用表达式 | 被借用对象的状态 |
| ---- | ---- |
| &const ident` | 变量 ident 只能被读不能被修改，也不能再对变量 ident 创建可变借用，允许对变量ident创建不可变借用 |
| &const "string literal" | 临时变量永远是 “只读” 状态 |
| &const (struct S) { ... } | 临时变量永远是 “只读” 状态 |
| &const e->field | e->field 进入 “只读” 状态，也不允许整体修改 *e。但允许修改 e 指向的其它成员，或者对其它成员做可变借用 |
| &const e.field | e.field 进入 “只读” 状态，也不允许整体修改 e。但允许修改 e 的其它成员，或者对其它成员做可变借用 |
| &const e[index] | e 进入 “只读” 状态，不允许修改 e 及其直接或间接成员，或者对其它成员做可变借用 |
| &const *e | *e 进入 “只读” 状态，不允许修改 *e 及其直接或间接成员，或者对其它成员做可变借用，如果 e 是 owned 指针类型，则 e 也进入只读状态 |

##### 4.2 可变借用对被借用对象的影响
对表达式 e 做可变借用， 即`&mut e`，表达式 e 进入 “冻结” 状态。在这个可变借用的生命周期结束之前，e 不能读，不能修改（包含被move），也不能被借用。
| 可变借用表达式 | 被借用对象的状态 |
| ---- | ---- |
| &mut ident` | 变量 ident 被冻结 |
| &mut "string literal" | 编译错误 |
| &mut (struct S) { ... } | 临时变量被冻结 |
| &mut e->field | e->field 被冻结，不允许读写 e->field，不允许整体修改 *e，但允许修改 e 指向的其它成员，或者对其它成员做可变借用 |
| &mut e.field | e.field 被冻结，不允许读写 e.field，不允许整体修改 e，但允许修改 e 的其它成员，或者对其它成员做可变借用 |
| &mut e[index] | e 被冻结，不允许读写 e 以及它的成员 |
| &mut *e | *e 被冻结，不允许读写 *e 以及它的成员，如果 e 是 owned 指针类型，则也不允许读写 e |

#### 5. 函数定义中包含借用类型
1. 不允许函数参数中没有借用类型的参数，但是函数返回是借用类型。

2. 如果函数参数中有一个借用类型的参数，函数返回是借用类型，那么我们直接认为这个返回值的借用，是来自于这个借用类型参数，返回的借用的 “被借用对象” 与这个借用类型参数的 “被借用对象” 一样，这个返回的借用也应该满足前面提到的那些借用规则。

3. 如果函数参数中有多个借用类型的参数，函数返回是借用类型，那么我们直接认为这个返回值的借用，同时包含了从多个借用类型参数传递过来的 “被借用变量”，这个返回的借用也应该满足前面提到的那些借用规则。

```C
int *borrow f1(int *borrow p) { return p; }
int *borrow f2(int *borrow p1, int *borrow p2) { return p1; }

void test() {
  int local = 5;
  int *borrow p1 = f1(&mut local);
  /* 函数 f1 的参数创建了一个对 local 的可变借用，这个借用被传递给了返回值 p1，
     导致 p1 相当于是对 local 的一个可变借用, 所以返回值 p1 的被借用对象是
     local, 在 p1 的生命周期结束之前，local 会一直被冻结。*/

  int local1, local2;
  int *borrow p2 = f2(&mut local1, &mut local2);
  /* 函数 f2 的参数创建了一个对 local1 和 local2
     的可变借用，这两个借用被传递给了返回值 p2， 导致 p2 相当于是对 local1 和
     local2 的一个可变借用, 所以返回值 p2 的被借用对象是 local1 和 local2, 在 p2
     的生命周期结束之前，local1 和 local2 一直被冻结。*/
}

int main() {
  test();
  return 0;
}
```

#### 6. struct定义中包含借用类型
1. 结构体的成员可以是借用类型，带有借用类型成员的结构体变量在定义的时候必须初始化。
2. 结构体内如果包含多个借用成员，那么这个结构体同时存在多个 “被借用对象”，这些借用成员也应该满足前面提到的那些借用规则。

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct R {
  int *borrow m1;
  int *borrow m2;
};

void test() {
  int local1, local2;
  struct R r = {.m1 = &mut local1, .m2 = &mut local2};
  // 在 r 的生命周期结束之前，local1 和 local2 一直被冻结。
  // 因为变量 r 在初始化的时候创建了一个对 local1 和 local2 的可变借用，
  // 导致 r 同时包含对 local1 的一个可变借用，也包含对 local2 的可变借用。
}

int main() {
  test();
  return 0;
}
```

#### 7. 借用变量的解引用操作
允许对借用指针变量解引用，与标准 C 的解引用操作一致：对借用指针变量`p`解引用的语法为 `*p`。
对`const T * borrow`类型的借用变量`e`解引用 `*e`，结果为`T`类型
对`T * borrow`类型的借用变量`e`解引用 `*e`，结果为`T`类型
如果`p`是指向`T`类型的借用，`o`是`T`类型的 lvalue，对于`*p`表达式，有如下限制：
| | T是Copy语义 | T是Move语义 |
| ---- | ---- | ---- |
| p是immut借用 | *p = expr; 不允许 | *p = expr; 不允许 |
| | o = *p; 允许 | o = *p; 不允许|
| p是mut借用 | *p = expr; 允许 | *p = expr; 允许 |
| | o = *p; 允许 | o = *p; 不允许|

上表中 move / copy 语义分别指： `T`是`owned`修饰的类型和`T`是其它类型。

注：上表中的赋值操作的权限，可同样应用于函数的传参和返回场景。

#### 8. 借用变量的成员访问
允许借用指针变量访问成员变量或调用成员函数，与标准 C 的箭头运算符一致：访问指针变量`p`的成员变量`field`的语法为`p->field`，调用指针变量`p`的成员方法`method()`的语法为`p->method()`。

##### 8.1 访问成员变量
通过借用访问成员变量，表达式的类型取决于成员变量本身的类型。`p->field` 表达式的类型与`field`成员定义的类型相同。
如果 `p->field` 的类型是`T`，`o`是`T`类型的 lvalue，对于 `p->field` 表达式，有如下限制：
| | T是Copy语义 | T是Move语义 |
| ---- | ---- | ---- |
| p是immut借用 | p->field = expr; 不允许 | p->field = expr; 不允许 |
| | o = p->field; 允许 | o = p->field; 不允许|
| p是mut借用 | p->field = expr; 允许 | p->field = expr; 允许 |
| | o = p->field; 允许 | o = p->field; 不允许|

上表中 move / copy 语义分别指： T 是 owned 修饰的类型和 T 是其它类型。

注：上表中的赋值操作的权限，可同样应用于函数的传参和返回场景。

##### 8.2 调用成员函数
通过借用调用成员函数，即 `p->method()` 的场景，实参和形参之间的规则如下：

|           | void method(const This * borrow this) | void method(This * borrow this) |     |
| --------- | ------------------------------------- | ------------------------------- | --- |
| p是immut借用 | 允许                                    | 不允许，immut借用不能创建出mut借用           |     |
| p是mut借用   | 允许，允许从mut借用创建immut借用                  | 允许                              |     |

举例来说：
```C
void int ::method1(const This *borrow this) {}
void int ::method2(This *borrow this) {}

void test() {
  int local = 5;
  const int *borrow p1 = &const local;
  int *borrow p2 = &mut local;
  // ok: 形参类型和实参类型一致，都是不可变借用
  p1->method1();
  // error: 形参是可变借用类型，实参是不可变借用类型，不可变借用不能创建出可变借用
  p1->method2();
  // ok: 形参是不可变借用类型，实参是可变借用类型，允许从可变借用创建出不可变借用
  p2->method1();
  // ok: 形参类型和实参类型一致，都是可变借用
  p2->method2();
}

int main() {
  test();
  return 0;
}
```

#### 9. 借用的类型转换
1. 对于任意类型 T，如果 T 实现了 trait TR，则允许指向类型 T 的借用向上转型为指向类型 TR 的借用；反过来，从类型 TR 的借用往类型 T 借用的转换，是不允许的。
```C
#include <stdio.h>

trait TR { void print(This * borrow this); };
void int ::print(int *this) { printf("%d\n", *this); }

impl trait TR for int;

void test() {
  int x = 10;
  int *borrow r = &mut x;
  // ok: 支持 int* 类型的借用向上转型为 trait TR* 类型的借用
  trait TR *borrow p = r;
  p->print();
  // error: 禁止 trait TR* 向下转型
  int *borrow px = (int *borrow)p;
}

int main() {
  test();
  return 0;
}
```
2. 允许指向类型 T 的借用转换为指向 void 类型的借用，反过来从类型 void 的借用往类型 T 借用的转换，是不允许的。
```C
void test() {
  int x = 10;
  int *borrow r = &mut x;
  void *borrow p = r;
  // error: 不允许转为 void *borrow 类型
  int *borrow t = (int *borrow)p;
}

int main() {
  test();
  return 0;
}
```
3. 只允许在非安全区进行`T * borrow`和`T *`之间的转换。
```C
int main() {
  // ok: 非安全区允许 T * borrow 和 T * 之间的转换
  int *borrow p = (int *borrow)NULL;
  // error: 安全区禁止 T * borrow 和 T * 之间的转换
  safe { int *borrow p = (int *borrow)NULL; }
  return 0;
}
```

#### 10. 借用的其它规则

除了上面的那些规则，对于借用，我们还有如下规则：
1. 对于全局变量，我们无法在函数签名中跟踪哪个函数读取了全局变量，哪个函数修改了全局变量。为了保证安全性，毕昇 C 规定：在安全区内，只允许对全局变量取只读借用，不允许取可变借用。如果是对函数名做借用，从生命周期的角度，可以当做是对全局变量做借用。

2. 借用变量在定义的时候必须初始化。
```C
void test() {
  // error: 必须初始化
  int *borrow p;
}

int main() {
  test();
  return 0;
}
```

3. 用一个借用类型的表达式给另外一个借用类型的 lvalue作初始化或再赋值，即`p = e`，`p`和`e`必须是同类型的借用类型，而且要求`e`的生命周期必须大于 p 的生命周期。
```C
#include <stdio.h>

void test() {
  int x = 1;
  int *borrow p = &mut x;
  {
    int y = 2;
    int *borrow pp = &mut y;
    // error: pp 生命周期小于 p
    p = pp;
    printf("%d\n", *p);
  }
  printf("%d\n", *p);
}

int main() {
  test();
  return 0;
}
```
基于此规则，一个`struct`内部的`borrow`指针成员，是不可以对这个`struct`或者它的其它成员做借用的。
```C
struct S {
  int m;
  const int *borrow p;
};

void test() {
  // error: 因为s.p的生命周期与s.m的生命周期相同
  struct S s = {.m = 0, .p = &const s.m};
}

int main() {
  test();
  return 0;
}
```

4. 借用变量不允许是全局变量，只能是局部变量。
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int g = 5;
// error: 借用变量不允许是全局变量
int *borrow p = &mut g;
void test() { int *borrow p = &mut g; }

int main() {
  test();
  return 0;
}
```

5. 不允许对包含借用的表达式，再取借用。同理，借用类型`T* borrow`中，`T`本身及其成员不能是借用类型。
```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct R {
  int *borrow p;
};

void test() {
  int local = 5;
  // error: 不允许多级借用指针
  int *borrow *borrow p = &mut(&mut local);

  struct R r1 = {.p = &mut local};
  // error: r1 中已经包含了借用
  struct R *borrow r2 = &mut r1;
}

int main() {
  test();
  return 0;
}
```

6. 不允许为借用类型实现 trait。
```C
trait TR{};

// error: 不允许为借用类型实现 trait
impl trait TR for int *borrow;

int main() { return 0; }
```

7. 不允许为借用类型添加成员函数。
```C
// error: 不允许为借用类型添加成员函数
void int *borrow::f() {} 

int main() { return 0; }
```

8. union 的成员不允许是借用类型。
```C
// error: 借用指针不允许作为 union 成员
union U {
  int *borrow p;
};

int main() { return 0; }
```

9. 借用指针类型不能是泛型实参。

10. 借用指针变量不支持索引运算。

11. 借用指针变量不支持算术运算。

12. 允许同类型的借用变量之间，使用 `==`、`!=`、`>`、`<`、`<=`、`>=` 等比较运算符操作。

13. 允许对借用类型使用 `sizeof`、`alignof`操作符，并且有：
`sizeof(T* borrow) == sizeof(T*)`
`_Alignof(T* borrow) == _Alignof(T*)`

14. 允许对借用类型使用一元的`&`、`!`及二元的`&&`、`||`运算符。

15. 不允许对借用类型使用一元的`-`、`~`、`&const`、`&mut`、`[]`、`++`、`--`运算符，也不允许对借用类型使用二元的`*`、`/`、`%`、`&`、`|`、`<<`、`>>`、`+`、`-`运算符。

16. 如果一个借用指针变量指向的是函数，那么可以通过这个借用指针变量来调用函数。
```C
#include <stdio.h>

void f() { printf("f()\n"); }

void test() {
  // ok: 对函数取不可变借用
  void (*borrow const p)() = &const f;
  p();
}

int main() {
  test();
  return 0;
}
```

17. 不允许对函数做可变借用，只能做只读借用。

## owned struct 类型

`owned struct` 是一种自定义类型，与 `struct` 不同，主要体现在非拷贝语义上，一律是 `move` 语义整体跟踪。这意味着原变量中的资源所有权会转移到新变量或参数中。本节依次介绍如何定义 `owned struct` 类型，如何创建 `owned struct` 实例。

### 定义 `owned struct` 类型
`owned struct` 类型的定义由关键字 `owned struct` 和自定义的名字组成，紧跟着是定义在一对花括号中 `owned struct` 定义体，在定义体中可以定义成员变量、析构函数、成员函数和访问修饰符。允许定义泛型 `owned struct`。
示例：
```c
#include <stdio.h>

owned struct Person {
public:
  char name[50];
  int age;
  char *getName(Person *this) { return this->name; }
  int getAge(Person *this) { return this->age; }
  ~Person(Person this) {
    printf("Name = %s\n", this.getName());
    printf("Age = %d\n", this.getAge());
  }
};

int main() {
  Person p = {.name = "Tom", .age = 18};
  return 0;
}
```
上例定义了名为 `Person` 的 `owned struct` 类型，它有两个成员变量 `name` 和 `age`、成员函数`getName` 和 `getAge`、以及析构函数 `~Person`。

注：`owned struct` 与 `C struct`类似，允许结构体嵌套。
#### 成员变量

`owned struct` 的成员变量分为实例成员变量和静态成员变量(由`static` 修饰)，和 `struct` 一样，成员变量定义的时候不允许写初始化。

```c
#include <stdio.h>

owned struct Person{
public:
    char name[50];
    static int age;
};

int main() {
  Person::age = 18;
  printf("%d\n", Person::age);
  return 0;
}
```

#### 析构函数
在一个对象生命周期结束时，会自动调用析构函数。析构函数与 `owned struct` 类型同名，前面带有 `~`, 但是不允许写泛型参数。当用户未定义析构函数时，编译器会自动提供默认的析构函数。
```c
#include <stdio.h>

owned struct S {
  ~S(S this) { printf("S destructor\n"); }
};

int main() {
  S s = {};
  return 0;
}
```
当`owned struct`内有owned 修饰的指针的时候，该指针需要在析构函数内手动释放
```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

owned struct Person {
public:
  int *owned age;
  ~Person(Person this) {
    printf("%d\n", *this.age);
    safe_free((void *owned)this.age);
  }
};

int main() {
  Person p = {.age = safe_malloc(18)};
  return 0;
}
```


析构函数的使用有以下限制：

+ 一个 `owned struct` 最多只能有一个析构函数。
+ 不允许作为扩展定义，不允许写返回类型，参数只有一个 `owned struct` 类型的 `this`, 不允许用户主动调用。
+ 析构函数内部需要将 `this` 的所有 `owned` 成员释放，规则同 `ownership` 的检查规则。需要注意的是如果 `this` 的成员是 `owned struct`类型，那么成员的释放是由编译器自动添加的。
+ 全局变量的析构函数不会被调用，包括函数内定义的 `static` 变量。
+ 局部变量的析构函数, 如果在本函数内没有被 `move` 走，析构的顺序为先定义的变量后析构。
+ 临时变量是不支持析构的，会报内存泄漏。
+ 自定义类型内部的成员带有析构函数的时候，析构函数调用顺序：
  + 先调用外层类型的析构函数，再调用成员变量的析构函数
  + 不保证各个成员的析构函数的调用顺序
+ `owned struct` 及其内部的成员变量，在作用域结束时，必须处于以下两种状态之一
  + `owned` 状态：`owned struct` 及其内部所有`owned`修饰的成员（1.`owned struct` 也属于`owned` 修饰的成员 2.递归包含）均未被移动
  + `moved` 状态：`owned struct` 作为一个整体已被显式移动
+ `owned struct` 及其内部的成员变量，在作用域结束时，如若处于如下状态，则报错。 报错信息模板为 partially moved `owned struct`:`%0` at scope end, `%1` moved"（`%0` 为`owned struct` 变量名，`%1` 为被移动的成员变量名）
  1. 有成员变量被转移所有权的状态： `owned struct`（递归包含嵌套成员）至少有一个`owned` 修饰的成员被移动。结构体本身未被整体移动。
+ `owned struct`内 `owned` 修饰的指针需要在析构函数内被手动释放。如未被手动释放，则报错。 报错模板信息为 destructor for`%0` incorrect, %1 of owned type and needs to be handled manually（ `%0` 为 `owned struct` 变量名）
#### 成员函数
`owned struct` 的成员函数分为实例成员函数和静态成员函数(不允许 `static` 修饰)。实例成员函数需要显式参数 `this`, 假设当前 `owned struct` 类型是 `C`, `this` 的类型可以是 `C*`、`const C*`、`C* borrow`、`const C * borrow`、`C * owned`。静态成员函数是没有 `this`。成员函数的访问与 `struct` 扩展成员函数访问一样(详见成员函数章节)。

注：当前不支持 `owned struct` 内部定义泛型函数。

```c
#include <stdio.h>

owned struct Person {
public:
  char name[50];
  int age;
  char *getName(Person *this) { return this->name; }
  char *getTypeName() { return "Preson"; }
};

int main() {
  Person p = {.name = "Tom", .age = 18};
  printf("TypeName: %s\nInstance Name: %s\n", Person::getTypeName(),
         p.getName());
  return 0;
}
```

上例是 `owned struct` 定义体内部实例成员函数 `getName` 和静态成员函数 `getTypeName`。

`owned struct` 定义体内部可以只有函数声明，把函数体外置。等同于 `owned struct` 定义体内部的成员函数。
示例：

```c
#include <stdio.h>

owned struct Person {
public:
  char name[50];
  int age;
  char *getName(Person *this);
  char *getTypeName();
};
char *Person::getName(Person *this) { return this->name; }
char *Person::getTypeName() { return "Preson"; }

int main() {
  Person p = {.name = "Tom", .age = 18};
  printf("TypeName: %s\nInstance Name: %s\n", Person::getTypeName(),
         p.getName());
  return 0;
}
```
`owned struct` 与 `struct` 类似，允许扩展成员函数（详见成员函数章节）。

#### 访问控制控制权限
`owned struct` 定义体内，允许成员指定可见性分为 `public` 和 `private`, 默认为 `private`。只有 `owned struct` 定义体内部的成员函数有权访问 `private` 和 `public`成员，在`owned struct` 外部（包括扩展成员函数）, 只能访问 `public` 成员。示例：

```c
owned struct A{
    private:
    int a;
    public:
    int b;
};
int A::f(A* this) {
    this->a; // error
    this->b; // ok
    return 0;
}
```

### 创建 `owned struct`实例
`owned struct` 允许使用 `struct initializer` 语法创建实例，也允许单独对每个成员变量初始化（如果成员变量是 `public`, 此时与 `struct` 一样单独跟踪每个成员的初始化状态，但是需要在安全区状态下保证在发生 `move`、传参、析构和返回等场景下该变量一定已经完整初始化, 非安全区不做保证。同时为了方便 **`owned struct` 类型在声明、定义时不携带 `owned struct` 关键字**。

## 标准库

### 安全 API

#### `safe_malloc`

`safe_malloc`是 BiShengC 语言提供的一个安全的内存分配函数。
该函数接收一个泛型类型`T`的变量，表示要分配的内存的大小以及分配后对内存的初始化。
该函数的返回值为`T * owned`类型，即指向分配好的堆内存的`owned`指针。
一些具体的使用例子如下。

在 C 语言中，如果我们需要申请一段堆内存，我们可以使用`malloc`函数进行分配，然后给该内存赋值，如：

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
}
```

然而，这样分配的内存不会在`p`的作用域结束时检查是否调用了`free`进行释放，会造成内存泄漏。
此外，如果再使用一个指针也指向该内存，并在作用域结束时释放，则会出现重复释放的问题，如：

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
    int *q = p;
    free(p);
    free(q); // error: double free!
}
```

使用 BiShengC 语言即可解决这种问题，相应的代码如下：

```c
safe void example(void) {
    int * owned p = safe_malloc(2);
    int * owned q = p;
    unsafe {
        safe_free((void * owned)q);
    }
}
```

在使用 BiShengC 语言改写后的代码中，如果我们在函数退出前什么都不做，则会出现编译错误`"memory leak of value: `q`"`，避免了内存泄漏问题的发生；
如果我们在函数退出前同时调用`safe_free((void * owned)p)`和`safe_free((void * owned)q)`，则会出现编译错误`"use of moved value: `p`"`，避免了重复释放问题的发生。

那么对于更为复杂的结构体类型，该如何正确使用`safe_malloc`进行内存分配呢？
对于结构体类型，需要首先在栈上构造出相应的变量，然后传给`safe_malloc`在堆上完成相应内存的分配，以下代码为具体示例：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void example(void) {
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * owned sp = safe_malloc(s);
    ...
}
```

#### `safe_free`

`safe_free`是 BiShengC 语言提供的一个安全的内存释放函数。
该函数接收一个`void * owned`类型的指针，表示要释放的内存的地址。
该函数的返回值为`void`类型。
因此，在调用`safe_free`进行释放前需要将`owned`指针显式地强制转换为`void * owned`类型，具体的转换规则可参考 [3.3](#33-强制类型转换) 节。
一些具体的使用例子如下。

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void example(void) {
    int * owned pa = safe_malloc(199);
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * owned sp = safe_malloc(s);
    unsafe {
        safe_free((void * owned)pa);
        safe_free((void * owned)sp->p);
        safe_free((void * owned)sp->q);
        safe_free((void * owned)sp); // 必须先释放 sp->p 和 sp->q，才能释放 sp
    }
}
```

#### `safe_swap`

`safe_swap`是 BiShengC 语言提供的一个安全交换两个变量的值的函数。
该函数是一个泛型函数,接收两个类型为`T* borrow`类型的参数,即需要交换的变量的值的借用。
该函数的返回值为`void`类型,该 API 的主要作用为在交换两个变量的值时,同时能交换两个变量所拥有的所有权.
一个具体的使用例子如下。

```c
owned struct S {
public:
    int* owned p;
    int* owned q;
    ~S(S this) {
        safe_free((void* owned)this.p);
        safe_free((void* owned)this.q);
    }
};

safe void example(void) {
    S s1 = { .p = safe_malloc(1), .q = safe_malloc(2) };
    S s2 = { .p = safe_malloc(3), .q = safe_malloc(4) };
    safe_swap(&mut s1, &mut s2); // 交换后,s1.p为3,s1.q为4
}
```

#### `forget`

`forget` 主要用于获取变量的所有权并且“忘记”它，该函数是一个泛型函数，接收一个类型为泛型类型`T`的变量，表示要“忘记”的值：
1. 如果该变量是 owned 指针，那么该指针指向的内存不会被释放；
2. 如果该变量是 owned struct 类型，那么不会调用其析构函数。

在一些特殊的场景，用户希望取得变量的所有权而不通过该变量来释放管理的底层资源（如堆内存或文件句柄，这些资源可能已经通过裸指针操作被转移或释放），例如：

```c
#include "bishengc_safety.hbs"
#include <string.h>
owned struct Resource {
public:
    char *owned s;
    ~Resource(This this) {
        safe_free((void *owned)this.s);
    }
};

void get_resource(char* val) {
    Resource r = { .s = safe_malloc<char>(100) };
    memcpy(val, (const void *)&r, sizeof(Resource)); // Resource中的资源被转移
    forget<Resource>(r); //此时 forget 函数会获取 r 的所有权，但是并不会调用 Resource 的析构函数来释放堆内存
}

int main() {
    char val[sizeof(Resource)];
    get_resource(val);
    return 0;
}
```
