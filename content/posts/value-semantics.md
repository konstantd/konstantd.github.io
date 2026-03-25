+++
date = '2026-03-24T15:14:39+01:00'
draft = false
title = 'How to Get Rid of Runtime-Dispatch with Modern Value Semantics'
+++



```cpp
class Vehicle {
    public:
        virtual ~Vehicle() = default;
        virtual void park() const = 0; // Deactivate it for the Vehicle class, but make it mandatory for the derived classes
    };
    

    class Bike : public Vehicle { 
        bool isElectric;
    public:
        void park() const override { 
            std::cout << "Bike can be parked in a bike rack.\n"; 
        }
    };
    
    class Car : public Vehicle {
        std::string plate;
    public:
        void park() const override { 
            std::cout << "Car with " << plate << " needs a standard spot.\n"; 
        }
    };
    
    class Truck : public Vehicle {
    public:
        void park() const override { 
            std::cout << "Truck needs more space.\n"; 
        }
    };
```