// joltphysics.i - SWIG interface file for JoltPhysics C# bindings
%module(directors="1") JoltPhysicsModule

// Enable directors for callback classes
%feature("director") JoltBroadPhaseLayerInterface;
%feature("director") JoltObjectVsBroadPhaseLayerFilter;
%feature("director") JoltObjectLayerPairFilter;
%feature("director") JoltContactListener;
%feature("director") JoltBodyActivationListener;

// C# module configuration - add using directives to all generated files
%pragma(csharp) moduleimports=%{
using System;
using System.Runtime.InteropServices;
%}

%typemap(csimports) SWIGTYPE %{
using System;
using System.Runtime.InteropServices;
%}

// Include standard typemaps
%include "stdint.i"
%include "typemaps.i"

// Tell SWIG about the C++ code to include in the wrapper
%{
#include "joltphysics_helpers.h"
%}

// ============================================================================
// Handle void* shape as IntPtr in C#
// ============================================================================

%typemap(cstype) void* "global::System.IntPtr"
%typemap(csin) void* "$csinput"
%typemap(csout) void* { return $imcall; }
%typemap(imtype) void* "global::System.IntPtr"

// ============================================================================
// Suppress operator warnings (use Equals/GetHashCode in C# instead)
// ============================================================================
%ignore JoltBodyID::operator==;
%ignore JoltBodyID::operator!=;

// ============================================================================
// Parse the header and generate bindings
// ============================================================================

%include "joltphysics_helpers.h"
