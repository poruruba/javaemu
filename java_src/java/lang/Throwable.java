package java.lang;

import base.framework.System;

public class Throwable {
	private String message;

	public Throwable() {
		message = "";
	}

	public Throwable( String message ){
		this.message = new String( message );
	}

	public String toString(){
		return new String( System.getClassName( this ) + ": " + message );
	}

	public void printStackTrace(){
		System.printStackTrace();
	}
}
