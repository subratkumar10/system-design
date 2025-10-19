/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 * Click nbfs://nbhost/SystemFileSystem/Templates/Classes/Class.java to edit this template
 */
package com.systemdesign.javadesign.patterns;

import java.util.ArrayList;

/**
 *
 * @author subrprad
 */

import java.util.List;

public class Main {
    
    public static void main(String[] args) {
//        System.out.println("Hello World!");
//        
//        List<Integer> list  = new ArrayList();
    	
    	SingletonPattern singletonPattern = SingletonPattern.getInstance();
    	try {
			singletonPattern.clone();
		} catch (CloneNotSupportedException e) {
			
			System.out.println(String.format("exception : %s",e));
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
    	
    	
    	
    
        
    }
    
}
