+++
date = '2026-02-10T11:23:23+01:00'
draft = false
title = 'Templates Compilation'
+++



Templates complilation generate code at compile time, before the program is executed (Compile-time vs run-time compilation). Every different instantiation produces a separate function at compile time. This way we avoid writing duplication of the code. Also, we can avoid mistakes even on compilation time when we static assert them. As the name suggests, this checks takes place statically - on compilation. Like this, we avoid mistakes that might appear during runtime, when we should not instantiate a function of a specific type. Static-asserts was already introduced in C++11, though the we have some important imporvements in C++17 like CTAD and concepts in C++20. We may see some examples on other blogs. Another advantage is Zero-Cost Abstraction since we only use what we need.

Consider the following code. We have some registers from which we would like to read. We can read and cast the result to the type we need directly, without writing different functions.



```cpp

    template<typename TCast>
    TCast readRegEdge(const edge_reg* const reg) const {  // This is a const pointer to a const register
    
        // Avoids compilation directly - BRAM reads per word = 4 bytes
        static_assert(sizeof(TCast) <= 4, "BRAM access type too large (max 32-bit). Use the template as integrals or floats direcly.");

        if (!this->fmcEdgeDriver) {
            throw std::runtime_error("fmcEdgeDriver is null — did you initialize it?");
        }   

        TCast value;  
        if (edge_get(this->fmcEdgeDriver->edge_hdl, const_cast<edge_reg*>(reg), &value) != 0) { 
            // Avoid heap alloc on exceptions
            throw std::runtime_error(
                std::string("Failed to read from register: ") + reg->reg_attr->name +  " | EDGE error: " + btrain_fmc_bsim_strerror(fmcEdgeDriver)
            );
        }
    
        // Return the value as the requested type
        return value;
    }



    // Now we can read from different registers directly
    int val = readRegEdge<int>(regs.bfield);  
    double val = readRegEdge<double>(regs.bfield);

    // Or if we have an enum class for states
    enum class StateMode : uint8_t {
        Active = 0,
        Paused = 1
        Disabled = 2,
    };

    // we can take the enum directly 
    StateMode m = readRegEdge<StateMode>(regs.flag);

```


## Will the above code compile?

Check it again. The answer is no. But why?

A double occupies 8 bytes and it will fail the stati_assert. Then we should use float instead. This saves us from reading/writing to memory that belongs to other parts and avoids creating UB..


---

{{< social_icons_extend_with_subscribe >}}