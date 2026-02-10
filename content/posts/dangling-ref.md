+++
date = '2026-02-10T11:19:20+01:00'
draft = false
title = 'Dangling References in C++'
summary = 'const T& binds to everything but it is dangerous'
+++


Dangling references are a common pitfall in C++ that can lead to undefined behavior, crashes, or memory corruption. In this article, we explore a real-world example using a configuration manager class and identify the issue.

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
        configs["max_connections"] = 100;
        configs["timeout"] = 30;
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

    std::cout << "Max Connections: " << configManager.findConfig("max_connections") << std::endl;
    std::cout << "Timeout: " << configManager.findConfig("timeout") << std::endl;
    std::cout << "Unknown Config: " << configManager.findConfig("unknown") << std::endl;

    return 0;
  }

```




## The problem:

The function 'findConfig' retrieves a configuration value from a std::map of settings. If the requested setting exists, it returns its value. If the setting is missing, it returns a default value (-1). Have you seen now the error? If name exists in the configs map, it returns a reference to the stored int, which is valid. If name does not exist, it returns -1. However, -1 is a temporary (rvalue). Returning a reference to a temporary leads to a dangling reference after the function returns.

When we call it with "unknown" as the name, it returns -1, the function ends, and the temporary -1 is destroyed. The returned reference now points to invalid memory (dangling reference). Undefined behavior occurs — possible crash, garbage value, or unexpected results. We should be careful when a function returns a reference.



---

{{< social_icons_extend_with_subscribe >}}