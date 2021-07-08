/** @file runme.cs
 * Entry point for BUSL, .NET version.
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
using org.tigris.busl;

public class runme
{
	/// <summary>
	///
	/// </summary>
	/// <param name="args"></param>
	/// <returns></returns>
	static int Main(string[] args) {
		bool changed = false;
		Busl s = new Busl();
		s.output = Console.Error;
		if (args.Length<1) {
			changed = s.usage("runme");
		}
		for (int i = 0; i < args.Length; ++i) {
			changed |= s.beautify(args[i]);
		}
		return s.finish(changed);
	}
}
