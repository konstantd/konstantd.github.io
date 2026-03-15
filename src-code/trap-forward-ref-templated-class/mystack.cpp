#include<vector>
#include<stdexcept>
#include<string>
#include<iostream>
#include <concepts>


template <typename T>
class MyStack{
    std::vector<T> data;

public:
    const T& top() const {
        if (data.empty()) throw std::out_of_range("Stack<>::top(): is empty");
        return data.back(); 
    }

    bool empty() const {
        return data.empty();
    }

    void pop() {
        if (data.empty()) throw std::out_of_range("Stack<>::pop(): is empty");
        data.pop_back();
    }

    template <typename U>
    requires std::constructible_from<T, U>
    void push(U&& elem) {
        data.emplace_back(std::forward<U>(elem));
    }
};

int main() {
    MyStack<std::string> st;
    std::string lval = "Lvalue";
    st.push("Temp");  // Calls it with rvalue
    st.push(lval);    // Calls it with lvalue
    std::cout << st.top() << "\n";

    return 0;
}