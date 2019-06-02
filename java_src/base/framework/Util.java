package base.framework;

public class Util {
	public native static final void byteArrayCopy( byte[] src, int srcOff, byte[] dest, int destOff, int length );
	public native static final void byteArrayFill( byte[] array, int arrayOff, int arrayLen, byte arrayValue );
	public native static final int byteArrayCompare( byte[] src, int srcOff, byte[] dest, int destOff, int length );

/*
	public static void byteArrayCopy( byte[] src, int srcOff, byte[] dest, int destOff, int length )
	{
		for( int i = 0 ; i < length ; i++ )
			dest[destOff + i] = src[srcOff + i];
	}

	public static void byteArrayFill( byte[] array, int arrayOff, int arrayLen, byte arrayValue )
	{
		for( int i = 0 ; i < arrayLen ; i++ )
			array[arrayOff + i] = arrayValue;
	}

	public static int byteArrayCompare( byte[] src, int srcOff, byte[] dest, int destOff, int length )
	{
		for( int i = 0 ; i < length ; i++ )
			if( src[srcOff + i] != dest[destOff + i] )
				return ((src[srcOff + i] < dest[destOff + i]) ? -1 : 1);
		
		return 0;
	}
*/

	public static int[] cloneIntArray( int[] array, int off, int len ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < len )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    int[] ret = new int[ len ];
    for (int i = 0; i < len; i++)
      ret[ i ] = array[ off + i ];
    return ret;
  }

  public static byte[] cloneByteArray( byte[] array, int off, int len ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < len )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    byte[] ret = new byte[ len ];
    for (int i = 0; i < len; i++)
      ret[ i ] = array[ off + i ];
    return ret;
  }

  public static short[] cloneShortArray( short[] array, int off, int len ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < len )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    short[] ret = new short[ len ];
    for (int i = 0; i < len; i++)
      ret[ i ] = array[ off + i ];
    return ret;
  }

  public static void setInt32( int value, byte[] array, int off ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < 4 )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    for( int i = 0 ; i < 4 ; i++ )
      array[ off + 3 - i ] = (byte) ( (value >> ( i * 8 ) ) & 0xFF);
  }

  public static int getInt32( byte[] array, int off ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < 4 )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    int x = 0;
    for( int i = 0 ; i < 4 ; i++ ){
      x <<= 8;
      x |= array[off + i] & 0x00ff;
    }

    return x;
  }

  public static int makeInt32(byte b1, byte b2, byte b3, byte b4 ){
    return ( ( b1 & 0x00ff ) << 24 ) | ( ( b2 & 0x00ff ) << 16 ) | ( ( b3 & 0x00ff ) << 8 ) | ( b4 & 0x00ff );
  }

  public static void setInt16( short value, byte[] array, int off ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < 2 )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    for( int i = 0 ; i < 2 ; i++ )
      array[ off + 1 - i ] = (byte) ( (value >> ( i * 8 ) ) & 0xFF);
  }

  public static short getInt16( byte[] array, int off ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < 2 )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    short s = 0;
    for( int i = 0 ; i < 2 ; i++ ){
      s <<= 8;
      s |= array[off + i] & 0x00ff;
    }

    return s;
  }

  public static short makeInt16(byte b1, byte b2){
    return (short)( ( ( b1 & 0x00ff ) << 8 ) | ( b2 & 0x00ff ) );
  }

  public static void setInt16Little( short value, byte[] array, int off ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < 2 )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    array[ off ] = (byte)( value & 0x00ff );
    array[ off + 1 ] = (byte)( ( value >> 8 ) & 0x00ff );
  }

  public static short getInt16Little( byte[] array, int off ){
    if( array == null )
      throw new java.lang.NullPointerException();
    if( array.length - off < 2 )
      throw new java.lang.ArrayIndexOutOfBoundsException();

    return (short)( ( ( array[ off + 1 ] & 0x00ff ) << 8 ) | ( array[ off ] & 0x00ff ) );
  }

  public static short makeInt16Little(byte b1, byte b2){
    return (short)( ( ( b2 & 0x00ff ) << 8 ) | ( b1 & 0x00ff ) );
  }

  public static void toHexChar( byte[] bArray, short offset, byte value ){
	  byte temp;
	  
	  temp = (byte)( ( value >> 4 ) & 0x0f );
	  if( temp >= 10 )
		  bArray[offset] = (byte)( temp - 10 + 'A' );
	  else
		  bArray[offset] = (byte)( temp + '0' );

	  offset++;
	  temp = (byte)( value & 0x0f );
	  if( temp >= 10 )
		  bArray[offset] = (byte)( temp - 10 + 'A' );
	  else
		  bArray[offset] = (byte)( temp + '0' );
  }
  
  public static String toHexString( byte[] bArray, short offset, short len ){
	  byte[] dest = new byte[len*2];
	  for( short i = 0 ; i < len; i++ )
		  toHexChar( dest, (short)(i * 2), bArray[offset + i] );
	  
	  return new String( dest );
  }
}
