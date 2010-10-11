/* Proto compiler
Copyright (C) 2009, Jacob Beal, and contributors 
listed in the AUTHORS file in the MIT Proto distribution's top directory.

This file is part of MIT Proto, and is distributed under the terms of
the GNU General Public License, with a linking exception, as described
in the file LICENSE in the MIT Proto distribution's top directory. */

// During experimental development, the NeoCompiler will be worked on
// "off to the side" of the original, not replacing it.

#include "config.h"
#include <fstream>
#include "nicenames.h"
#include "neocompiler.h"

/*****************************************************************************
 *  COMPILATION SEQUENCE                                                     *
 *****************************************************************************/
// Order of compilation:
// 1. Read a file or string into an S-expression
// 2. Interpret the S-expression into dataflow graph elements
//   [Repeat steps 1 & 2 until all files & sexprs have been read]
// 3. Optimize the dataflow graph
// 4. Emit the dataflow graph as platform-specific code
// 5. Optimize the platform-specific code

// Error behavior: in any given stage, do all the independent processing
// that's possible, flag if there's an error, and then quit

// len is filled in w. output length... eventually
uint8_t* NeoCompiler::compile(char *str, int* len) {
  last_script=str;
  compile_phase = "parsing"; // PHASE: text-> sexpr
  SExpr* sexpr = read_sexpr("command-line",str);
  compiler_error|=!sexpr; terminate_on_error();
  compile_phase = "interpretation"; // PHASE: sexpr -> IR
  interpreter->interpret(sexpr); // terminates on error internally
  if(is_early_terminate==3) 
    { *cperr << "Stopping before analysis" << endl; exit(0); }
  
  compile_phase = "analysis"; // PHASE: IR manipulation
  analyzer->analyze(interpreter->main); // terminates on error internally
  if(is_early_terminate==2) 
    { *cperr << "Stopping before localization" << endl; exit(0); }
  
  compile_phase = "legality check"; // PHASE: code emission
  Propagator *p = new CheckTypeConcreteness();
  p->propagate(interpreter->main);
  compile_phase = "localization"; // PHASE: Global-to-local transformation
  localizer->localize(interpreter->main); // terminates on error internally
  //compile_phase = "local analysis"; // PHASE: IR manipulation
  //analyzer->analyze(interpreter->main); // terminates on error internally
  if(is_early_terminate==1) 
    { *cperr << "Stopping before emission" << endl; exit(0); }
  
  compile_phase = "emission"; // PHASE: code emission
  return emitter->emit_from(interpreter->main, len);
}

/*****************************************************************************
 *  COMPILER OBJECT API                                                      *
 *****************************************************************************/
NeoCompiler::NeoCompiler(Args* args) {
  is_dump_all = args->extract_switch("-CDall");
  is_dump_code = args->extract_switch("--instructions") | is_dump_all;
  is_early_terminate = (args->extract_switch("--no-emission") ? 1 : 0);
  if(args->extract_switch("--no-localization")) is_early_terminate = 2;
  if(args->extract_switch("--no-analysis")) is_early_terminate = 3;
  last_script="";

  // Set up paths
  // srcdir is an undocumented option used for uninstalled execution
  string srcdir = (args->extract_switch("--srcdir"))?args->pop_next():"";
  if (args->extract_switch("--basepath"))
    proto_path.add_to_path(args->pop_next());
  else 
    proto_path.add_default_path(srcdir);
  while(args->extract_switch("-path",false)) // can extract multiple times
    proto_path.add_to_path(args->pop_next());
  
  interpreter = new ProtoInterpreter(this,args);
  analyzer = new ProtoAnalyzer(this,args);
  localizer = new GlobalToLocal(this,args);

  if(args->extract_switch("--to-biobricks")) {
    cerr << "ERROR: BIOBRICK EMITTER NO LONGER HARD-WIRED IN\n"; exit(0);
    //emitter = new BioBrickEmitter(this,args);
  } else { // default = protokernel
    emitter = new ProtoKernelEmitter(this,args);
  }
}

NeoCompiler::~NeoCompiler() {
  delete interpreter;
}

// When being run standalone, -D controls dumping (it's normally 
// consumed by the simulator).  Likewise, if -dump-stem is present,
// then dumping goes to a file instead of stdout
void NeoCompiler::init_standalone(Args* args) {
  compiler_test_mode = args->extract_switch("--test-mode");
  is_dump_code |= args->extract_switch("-D");
  bool dump_to_stdout = true;
  char *dump_dir = "dumps", *dump_stem = "dump";
  if(args->extract_switch("-dump-dir"))
    { dump_dir = args->pop_next(); dump_to_stdout=false; }
  if(args->extract_switch("-dump-stem"))
    { dump_stem = args->pop_next(); dump_to_stdout=false; }
  if(dump_to_stdout) {
    cpout=&cout;
  } else {
    char buf[1000];
    // ensure that the directory exists
    snprintf(buf, 1000, "mkdir -p %s", dump_dir);
    (void)system(buf);
    sprintf(buf,"%s/%s.log",dump_dir,dump_stem);
    
    cpout = new ofstream(buf);
  }
  if(compiler_test_mode) cperr=cpout;
  
  // in internal-tests mode: run each internal test, then exit
  if(args->extract_switch("--internal-tests")) {
    type_system_tests();
    exit(0);
  }
}

void NeoCompiler::set_platform(string path) {
  cerr << "WARNING: NeoCompiler platform handling not yet implemented\n";
}