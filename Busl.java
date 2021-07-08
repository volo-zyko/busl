/** @file Busl.java
 * Java wrapper class vor BUSL.
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

package org.tigris.busl;

/**
 * Busl main class.
 */
public class Busl {

	/**
	 * Output stream to be used by beautify(), usage() and finish() calls.
	 * If null then no output will appear, so then the only feedback
	 * provided is the return value of finish().
	 */
	public java.io.OutputStream output = null;

	/**
	 * Constructor.
	 */
	public Busl() {
		init();
		if (data == 0L) {
			// This should never happen
			throw new RuntimeException("Error in Busl initialization");
		}
	}

	/**
	 * Output usage instructions.
	 *
	 * @param command command name to be included in usage instructions
	 * @return true always
	 */
	public native final boolean usage(final byte[] command) throws java.io.IOException;

	/**
	 * Beautify a file
	 *
	 * @param name file to be beautified
	 * @return true if the beautify leaded to a file change
	 */
	public native final boolean beautify(final byte[] name) throws java.io.IOException;

	/**
	 * Clean up any files left open and output any remaining messages.
	 *
	 * @param changed if false then output the "no files modified" message.
	 * @return result of beautify process:
	 *    0 = All files beautified successfully without failures or warnings
	 *    1 = At least one file resulted in a failure
	 *    2 = There were some warnings but no failures
	 */
	public native final int finish(boolean changed) throws java.io.IOException;

	/**
	 * Clean up the state
	 */
	public native final void finalize();

	/**
	 * Output usage instructions
	 *
	 * @param command command name to be included in usage instructions
	 */
	public final boolean usage(String command) throws java.io.IOException {
		return usage(command.getBytes());
	}

	/**
	 * Beautify a file
	 *
	 * @param name file to be beautified
	 */
	public final boolean beautify(String name) throws java.io.IOException {
		return beautify(name.getBytes());
	}

	/**
	 * Main function of BUSL application.
	 *
	 * At the same time this is an example how to use the
	 * usage(), beautify() and finish() member functions.
	 * The idea is that the Busl object stores all current
	 * options of the beautify process. This means that
	 * e.g. beautify("x") has the same effect as providing
	 * "x" on the command line: BUSL will switch to XML
	 * mode and all following beautify() calls (using a
	 * valid file name as argument) will use it.
	 *
	 * @param args command line arguments
	 */
	public static final void main(String[] args) throws java.io.IOException {

		boolean changed = false;
		Busl s = new Busl();
		s.output = System.err; // provide output stream

		if (args.length < 1) {
			changed = s.usage("java -jar Busl.jar");
		}
		for (int i = 0; i < args.length; ++i) {
			changed |= s.beautify(args[i]);
		}
		System.exit(s.finish(changed));
	}

	private native void init();

	private long data = 0L;

	static {
		System.loadLibrary("busl");
	}

}
