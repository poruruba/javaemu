package java.lang;

public class Object {
	public Object(){
	}
	
	protected Object clone(){
		return new Object();
	}
	
	protected boolean equals( Object obj ){
		if( obj == this )
			return true;
		else
			return false;
	}
	public String toString(){
		return "";
	}
}
