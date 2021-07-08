/** @file Busl.cs
 * Main class for BUSL, .NET version.
 * Copyright (c) 2003-2009, Jan Nijtmans. All rights reserved.
 */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using System;
using System.IO;
using System.Runtime.InteropServices;

namespace org.tigris.busl {

	/// <summary>
	/// Main Busl class, storing state of every beautify operation.
	/// </summary>
	public class Busl: IDisposable {
		/// <summary>
		///
		/// </summary>
		public Busl() {
			gchandle = GCHandle.Alloc(this, GCHandleType.Weak);
			cPtr = Busl.busl_create(IntPtr.Zero, new WrtDelegate(wrt), (IntPtr) gchandle);
		}

#if DEBUG
		const string libname = "busld";
#else
		const string libname = "busl";
#endif

		/// <summary>
		///Beautifier resulted in a source-code change
		/// </summary>
		const int CHANGED = 1;

		/// <summary>
		///
		/// </summary>
		~Busl() {
			Dispose();
		}

		/// <summary>
		///
		/// </summary>
		public virtual void Dispose() {
			if (cPtr != IntPtr.Zero) {
				IntPtr saved = cPtr;
				cPtr = IntPtr.Zero;
				Busl.busl_delete(saved);
			}
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Output usage instructions
		/// </summary>
		/// <param name="argv0">first command line argument</param>
		/// <returns></returns>
		public bool usage(string argv0) {
			bool result = Busl.busl_usage(this.cPtr, argv0) != 0;
			if (e != null) {
				Exception exc = e;
				e = null;
				throw exc;
			}
			return result;
		}

		/// <summary>
		/// Handle a single command line argument. If the argument
		/// is a file name, this file is beautified. If the argument
		/// is a command line option, the status is changed such
		/// that following beautify operations take the option
		/// into account.
		/// </summary>
		/// <param name="filename">file name</param>
		/// <returns>true if the file is modified</returns>
		public bool beautify(string filename) {
			bool result = (Busl.busl_beautify(this.cPtr, filename)&CHANGED) != 0;
			if (e != null) {
				Exception exc = e;
				e = null;
				throw exc;
			}
			return result;
		}

		/// <summary>
		/// finish beautify operations. Output remaining messages,
		/// eventually delete temporary files and determine exit value.
		/// </summary>
		/// <param name="changed">true if at least one file is modified</param>
		/// <returns>exit value</returns>
		public int finish(bool changed) {
			int result = Busl.busl_finish(this.cPtr, changed? 1: 0);
			if (e != null) {
				Exception exc = e;
				e = null;
				throw exc;
			}
			return result;
		}

		/// <summary>
		/// Allocate memory for BUSL operations
		/// </summary>
		/// <param name="wrt"></param>
		/// <param name="obj"></param>
		/// <returns></returns>
		[DllImport(libname, EntryPoint = "busl_create")]
		private static extern IntPtr busl_create(IntPtr s, WrtDelegate wrt, IntPtr obj);

		/// <summary>
		/// Wrapper for busl_usage();
		/// </summary>
		/// <param name="obj"></param>
		/// <param name="arg"></param>
		/// <returns></returns>
		[DllImport(libname, EntryPoint = "busl_usage", CharSet = CharSet.Ansi)]
		private static extern int busl_usage(IntPtr obj, string arg);

		/// <summary>
		/// Wrapper for busl_beautify().
		/// </summary>
		/// <param name="obj"></param>
		/// <param name="msg"></param>
		/// <returns></returns>
		[DllImport(libname, EntryPoint = "busl_beautify", CharSet = CharSet.Ansi)]
		private static extern int busl_beautify(IntPtr obj, string msg);

		/// <summary>
		/// Wrapper for busl_finish().
		/// </summary>
		/// <param name="obj"></param>
		/// <param name="jarg2"></param>
		/// <returns></returns>
		[DllImport(libname, EntryPoint = "busl_finish")]
		private static extern int busl_finish(IntPtr obj, int jarg2);

		/// <summary>
		/// Free memory
		/// </summary>
		/// <param name="s"></param>
		[DllImport(libname, EntryPoint = "busl_delete")]
		private static extern void busl_delete(IntPtr s);

		/// <summary>
		/// Store address of Busl C struct.
		/// </summary>
		private IntPtr cPtr = IntPtr.Zero;

		/// <summary>
		/// Store address of Busl weak reference.
		/// </summary>
		private GCHandle gchandle;

		/// <summary>
		/// Remember last exception that occured. If an exception
		/// happened, further output is disabled until the
		/// beautify operation finishes.
		/// </summary>
		private Exception e = null;

		/// <summary>
		/// TextWriter where output messages should go to.
		/// </summary>
		public TextWriter output = Console.Error;

		/// <summary>
		/// Prototype for callback function
		/// </summary>
		private delegate void WrtDelegate(IntPtr obj, string message);

		/// <summary>
		/// Delegate used as callback function
		/// </summary>
		/// <param name="obj"></param>
		/// <param name="message"></param>
		private static void wrt(IntPtr obj, string message) {
			Busl s = (Busl) ((GCHandle) obj).Target;
			if ((s != null) && (s.e == null)) {
				try {
					s.output.Write(message);
				} catch (Exception e) {
					s.e = e;
				}
			}
		}
	}
}
