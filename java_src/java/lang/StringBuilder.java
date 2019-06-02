package java.lang;

import base.framework.*;

public class StringBuilder {
  String strings[] = new String[4];
  int count = 0;

  /** Constructs an empty string buffer. */
  public StringBuilder() {
  }

  /** Constructs a string buffer containing the given string. */
  public StringBuilder(String s) {
    append(s);
  }

  /**
   * Constructs a string buffer containing the string representation of the
   * given boolean value.
   * @see waba.sys.Convert
   */
  public StringBuilder append(boolean b) {
    return append(Convert.toString(b));
  }

  /**
   * Constructs a string buffer containing the string representation of the
   * given char value.
   * @see waba.sys.Convert
   */
  public StringBuilder append(char c) {
    return append(Convert.toString(c));
  }

  /**
   * Constructs a string buffer containing the string representation of the
   * given int value.
   * @see waba.sys.Convert
   */
  public StringBuilder append(int i) {
    return append(Convert.toString(i));
  }

  /** Appends the given character array as a string to the string buffer. */
  public StringBuilder append(char c[]) {
    return append(new String(c));
  }

  /** Appends the given string to the string buffer. */
  public StringBuilder append(String s) {
    if (s == null)
      return this;
    if (strings.length == count) {
      String newStrings[] = new String[strings.length * 2];
      System.arraycopy(strings, 0, newStrings, 0, strings.length);
      strings = newStrings;
    }
    strings[count++] = s;
    return this;
  }

  /** Appends the string representation of the given object to the string buffer. */
  public StringBuilder append(Object obj) {
    return append(obj.toString());
  }

  /**
       * Empties the StringBuffer and clears out its contents so it may be reused. The
   * only value that can be passed to this method is 0. The method is called setLength()
   * for compatibility reasons only.
   */
  public void setLength(int zero) {
    count = 0;
  }

  public int length(){
    int length = 0;
    for( int i = 0 ; i < count; i++ ){
      if (strings[i] == null)
        return length;
      else
        length += strings[i].length();
    }
    return length;
  }

  /** Converts the string buffer to its string representation. */
  public String toString() {
    return new String(strings, count);
  }
}
