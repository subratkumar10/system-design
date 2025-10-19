/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 */

package com.mycompany.demo;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 *
 * @author subrprad
 */

interface Animal {
    void speak();
}

// Classes implementing the interface
class Dog implements Animal {
    public void speak() {
        System.out.println("Woof!");
    }
}

class Cat implements Animal {
    public void speak() {
        System.out.println("Meow!");
    }
}

public class Demo {

    public static void main(String[] args) {
//        System.out.println("Hello World!");

    List<Integer> list = new ArrayList<>();
    
    Set<Integer>set1 = new TreeSet<>((a,b) -> (b-a));
    
    
    for(int i=5;i>=1;i--){
        set1.add(i);
    }
    for(Integer val: set1){
        System.out.println(val);
    }

        Dog d = new Dog();
        Cat c = new Cat();
        d.speak();
        c.speak();
    }
}
