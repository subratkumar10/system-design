package com.systemdesign.javadesign.patterns;

import java.io.Serializable;

public class SingletonPattern implements Cloneable, Serializable {

	private volatile static SingletonPattern singletonObject;

	private SingletonPattern() {
		if (singletonObject != null)
			throw new IllegalStateException("Object already created");
	}

	public static SingletonPattern getInstance() {
		if (singletonObject == null) {
			synchronized (SingletonPattern.class) {
				if (singletonObject == null) {
					singletonObject = new SingletonPattern();
				}
			}
		}
		return singletonObject;
	}

	protected Object clone() throws CloneNotSupportedException {
		throw new CloneNotSupportedException();
	}

	protected Object readResolve() {
		return singletonObject;
	}

	public void doSomeWork() {
		// do some work
	}

}
