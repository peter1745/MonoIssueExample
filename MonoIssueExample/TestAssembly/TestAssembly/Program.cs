using System;

namespace MyAssembly
{
	public class MyClass
	{
		public float MyVar;
	}

	public class AnotherClass
	{
		private MyClass m_MyClass;

		public void CalledViaUnmanagedThunk(float variable)
		{
			Console.WriteLine("Hello from Unmanaged Thunk!");
			m_MyClass.MyVar = variable;
		}

		public void CalledViaRuntimeInvoke(float variable)
		{
			Console.WriteLine("Hello from Runtime Invoke!");
			m_MyClass.MyVar = variable;
		}
	}

}
