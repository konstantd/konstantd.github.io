+++
date = '2026-01-07T11:19:20+01:00'
draft = false
title = "The Most Dangerous Pattern in C++: Why `const T&` Is Breaking Your Code"
summary = 'const T& binds to everything but it is dangerous'
tags = ["advanced-level", "move-semantics", "undefined-behaviour", "ref-qualifiers", "performance"]
+++

Dangling references are a common pitfall in C++ that can lead to undefined behavior, crashes, or memory corruption. We will see shortly how to spot a dangling reference in a simple example, and later we will delve a bit deeper on the topic and see what we can do to protect ourselves.




Consider the following code. Can you spot the error? Give yourself 1-2 minutes before you scroll further down.


```cpp

  #include <iostream>
  #include <map>
  #include <string>

  class ConfigManager {
  private:
    std::map<std::string, int> configs;

  public:
    ConfigManager() {
        configs["max_tries"] = 10;
        configs["timeout_msecs"] = 30;
    }

    const int& findConfig(const std::string& name) const {  
        auto it = configs.find(name);  
        if (it != configs.end()) 
            return it->second; 
        
        return -1;
    }
  };

  int main() {
    ConfigManager configManager;

    std::cout << "Max Connections Tries: " << configManager.findConfig("max_tries") << std::endl;
    std::cout << "Timeout in msecs: " << configManager.findConfig("timeout_msecs") << std::endl;
    std::cout << "Unknown Config: " << configManager.findConfig("unknown") << std::endl;

    return 0;
  }

```




## The problem:

The function 'findConfig' retrieves a configuration value from a std::map of settings. If the requested setting exists, it returns its value as a `const int&`. If the setting is missing, it returns a default value (-1). Have you seen now the error? If name exists in the configs map, it returns a reference to the stored int, which is valid. If name does not exist, it returns -1. However, -1 is a `temporary (rvalue)`. **Returning a reference to a temporary leads to a dangling reference after the function returns.** This is a possible crash, garbage value, or unexpected results. We should be careful when a function returns a reference.


The compiler will also catch it as a `warning`.


```bash
dangling-config-map-ref.cpp: In member function ‘const int& ConfigManager::findConfig(const string&) const’:
dangling-config-map-ref.cpp:20:14: warning: returning reference to temporary [-Wreturn-local-addr]
   20 |       return -1;
      |              ^~
```

---

## A Constant Lvalue Reference Binds to Everything, but Does Not Extend the Lifetime 

While it is true that `const T&` (a constant lvalue reference) is really flexible, it cannot extend the lifetime of the object it binds to. It is a common misconception that when using a constant lvalue reference we are safe. 

**We should never assume that a reference will keep a temporary alive once the execution moves to the next line.** Though, we can solve this problem by creating a 
variable that is part of the class.


```cpp
inline static const int NOT_FOUND = -1;
```

This will be instantiated only once, since it is static and now it is safe to return this as a reference.

```cpp

  #include <iostream>
  #include <map>
  #include <string>

  class ConfigManager {
  private:
    std::map<std::string, int> configs;
    inline static const int NOT_FOUND = -1;
  public:
    ConfigManager() {
        configs["max_tries"] = 10;
        configs["timeout_msecs"] = 30;
    }

    const int& findConfig(const std::string& name) const {  
        auto it = configs.find(name);  
        if (it != configs.end()) 
            return it->second; 

        return NOT_FOUND;
    }
  };

  int main() {
    ConfigManager configManager;

    std::cout << "Max Connections Tries: " << configManager.findConfig("max_tries") << std::endl;
    std::cout << "Timeout in msecs: " << configManager.findConfig("timeout_msecs") << std::endl;
    std::cout << "Unknown Config: " << configManager.findConfig("unknown") << std::endl;

    return 0;
  }

```

## Leveling Up: API Protection with Reference-Qualifiers  

The above is a simple example, for the sake of demonstration of `rvalue references`. Returing an int by value would not hurt the performance at all. We can step up our game now and see a practical example where we are moving around bigger objects without hurting performance.


```cpp
class DataManager {
    std::vector<std::string> big_data;
public:
    // This copies either for Lvalues or Rvalues
    std::vector<std::string> getData() {
        return big_data; 
    }
};
```


With the above class everything is a copy:


```cpp
// COPY of the Lvalue
DataManager dm;
std::vector<std::string> myData = dm.getData(); 

// COPY even if it is Rvalue
std::vector<std::string> myData = DataManager().getData(); 
```


Though, we can explicitly use `ref-qualifiers` on our API to tell the compiler when it is allowed to copy or move. 
The default behaviour (when we are not using qualifiers) actually tells the compiler to treat this function as a catch-all, since a constant lvalue reference can bind to lvalues, const lvalues and rvalues. That's why both calls above result to copies. Note that **RVO does NOT apply to Members of a class**, so we cannot just return the name of the vector in the member function. We saw [here](#dont-break-nrvo.md) that RVO applies to local objects inside a function's scope. 


Once we define even one ref-qualifier to one version of the function, the unqualified version no longer exists.
Think of it as it is with the special member functions of a class. (Once you write for instance a Destructor, the compiler disables the default Move Constructor and Move Assignment.) So here we should specify one function for the copies and one for the moves.


```cpp
class DataManager {
    std::vector<std::string> big_data;
public:
    // LVALUE: The object stays alive. We MUST copy the vector.
    std::vector<std::string> getData() const & {
        return big_data; 
    }

    // RVALUE: The object is a temporary at that point. We can MOVE the vector!
    std::vector<std::string> getData() && {
        return std::move(big_data); 
    }
};
```

And now on the above code we have a huge performance improvemnet:

```cpp
// COPY of the Lvalue
DataManager dm;
std::vector<std::string> myData = dm.getData(); 

// Moves as DataManager().getData() is a temporary and will be destroyed anyways!
std::vector<std::string> myData = DataManager().getData(); 
```




---

{{< social_icons_extend_with_subscribe >}}