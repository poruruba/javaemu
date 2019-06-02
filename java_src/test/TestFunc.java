package test;

import base.framework.System;
import base.framework.Convert;
import base.framework.Util;

public class TestFunc{
  public static void main( String[] args ){
  	try
  	{
  		System.println("Hello test/TestFunc");

  		if( args.length >= 1 )
  			System.println("args[0]=" + args[0]);
  		
  		String[] input = System.getInput(-1);
  		for( int i = 0 ; i < input.length ; i++ )
	  		System.println("params[" + i+ "]=" + input[i]);

  		System.setOutput(new String[]{ "World", "こんばんは" });
  	}catch( Exception ex )
  	{
		System.print( ex.toString() );
  		ex.printStackTrace();
  	}
  }
}
