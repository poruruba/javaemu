package base.framework;

public class System {
	public native static void arraycopy( Object src, int srcOff, Object dest, int destOff, int length );

	public native static final void gc();
	public native static final Object newInstance( String className );

	public native static final String getClassName( Object obj );
	public native static final boolean hasClass( String name );

	public native static int sleep( int interval );

	public native static final String[] getInput(int max);
	public native static final void setOutput(String[] output);

	public native static void printStackTrace();
	public native static void print( String s );

	public static void println( String s ){
		print( s + "\n");
	}
}
