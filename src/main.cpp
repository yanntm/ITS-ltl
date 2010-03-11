// Copyright (C) 2004, 2009, 2010 Laboratoire d'Informatique de Paris
// 6 (LIP6), d�partement Syst�mes R�partis Coop�ratifs (SRC),
// Universit� Pierre et Marie Curie.
//
// This file is part of the Spot tutorial. Spot is a model checking
// library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Spot; see the file COPYING.  If not, write to the Free
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include <iostream>
#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <cstring>

#include "bdd.h"
#include "ltlparse/public.hh"
#include "ltlvisit/destroy.hh"

#include "sogtgbautils.hh"
#include "train.hh"
#include "MemoryManager.h"

// prod parser
#include "Modular2ITS.hh"
#include "prod/ProdLoader.hh"
#include "PNet.hh"

// fair CTL bricks
#include "tgbaIts.hh"
#include "fsltl.hh"

using namespace its;
using namespace sogits;

void syntax(const char* prog) {
  std::cerr << "Usage: "<< prog << " [OPTIONS...] petri_net_file " << std::endl
            << "where OPTIONS are" << std::endl
            << "Actions:" << std::endl
            << "  -aALGO          apply the emptiness check algoritm ALGO"
            << std::endl
            << "  -SSOGTYPE       apply the SOG construction algoritm SOGTYPE={SOG,SLOG,DSOG} (SLOG by default)"
            << std::endl
            << "  -C              display the number of states and edges of the SOG"
            << std::endl
            << "  -c              check the formula" << std::endl
	    << "  -dR3            disable the SCC reduction" << std::endl
	    << "  -R3f            enable full SCC reduction" << std::endl
            << "  -e              display a sequence (if any) of the net "
            << "satisfying the formula (implies -c)" << std::endl
            << "  -fformula       specify the formula" << std::endl
            << "  -Fformula_file  formula read from formula_file"
            << std::endl
            << "  -g              display the sog"
            << std::endl
            << "  -p              display the net"
            << std::endl
            << "  -s              show the formula automaton"
            << std::endl
            << "Options of the formula transformation:"
            << std::endl
            << "  -b              branching postponement"
            << " (false by default)" << std::endl
            << "  -l              fair-loop approximation"
            << " (false by default)" << std::endl
            << "  -x              try to produce a more deterministic automaton"
            << " (false by default)" << std::endl
            << "  -y              do not merge states with same symbolic "
            << "representation (true by default)"
            << std::endl
            << "Where ALGO should be one of:" << std::endl
            << "  Cou99(OPTIONS) (the default)" << std::endl
            << "  CVWY90(OPTIONS)" << std::endl
            << "  GV04(OPTIONS)" << std::endl
            << "  SE05(OPTIONS)" << std::endl
            << "  Tau03(OPTIONS)" << std::endl
            << "  Tau03_opt(OPTIONS)" << std::endl;
  exit(2);
}

int main(int argc, const char *argv[]) {

  // external block for full garbage
  {

  bool check = false;
  bool print_rg = false;
  bool print_pn = false;
  bool count = false;

  bool ce_expected = false;
  bool fm_exprop_opt = false;
  bool fm_symb_merge_opt = true;
  bool post_branching = false;
  bool fair_loop_approx = false;
  bool print_formula_tgba = false;

  bool scc_optim = true;
  bool scc_optim_full = false;

  std::string ltl_string = "1"; // true
  std::string algo_string = "Cou99";

  sog_product_type sogtype = SLOG;

  std::string pathprodff = argv[argc-1];

  int pn_index = 0;
  for (;;) {
    if (argc < pn_index + 2)
      syntax(argv[0]);

    ++pn_index;

    if (!strncmp(argv[pn_index], "-a", 2)) {
      algo_string = argv[pn_index]+2;
    }
    else if (!strcmp(argv[pn_index], "-b")) {
      post_branching = true;
    }
    else if (!strcmp(argv[pn_index], "-c")) {
      check = true;
    }
    else if (!strcmp(argv[pn_index], "-C")) {
      count = true;
    }
    else if (!strcmp(argv[pn_index], "-e")) {
      ce_expected = true;
    }
    else if (!strcmp(argv[pn_index], "-s")) {
      print_formula_tgba = true;
    }
    else if (!strncmp(argv[pn_index], "-f", 2)) {
      ltl_string = argv[pn_index]+2;
    }
    else if (!strncmp(argv[pn_index], "-dR3", 4)) {
      scc_optim = false;
    }
    else if (!strncmp(argv[pn_index], "-R3f", 4)) {
      scc_optim = true;
      scc_optim_full = true;
    }
    else if (!strncmp(argv[pn_index], "-F", 2)) {
      std::ifstream fin(argv[pn_index]+2);
      if (!fin) {
          std::cerr << "Cannot open " << argv[pn_index]+2 << std::endl;
          exit(2);
      }
      if (!std::getline(fin, ltl_string, '\0')) {
          std::cerr << "Cannot read " << argv[pn_index]+2 << std::endl;
          exit(2);
      }
    }
    else if (!strcmp(argv[pn_index], "-g")) {
      print_rg = true;
    }
    else if (!strcmp(argv[pn_index], "-l")) {
      fair_loop_approx = true;
    }
    else if (!strcmp(argv[pn_index], "-p")) {
      print_pn = true;
    }
    else if (!strcmp(argv[pn_index], "-SSOG")) {
      sogtype = PLAIN_SOG;
    }
    else if (!strcmp(argv[pn_index], "-SSLOG")) {
      sogtype = SLOG;
    }
    else if (!strcmp(argv[pn_index], "-SDSOG")) {
      sogtype = DSOG;
    }
    else if (!strcmp(argv[pn_index], "-SFSLTL")) {
      sogtype = FSLTL;
    }
    else if (!strcmp(argv[pn_index], "-x")) {
      fm_exprop_opt = true;
    }
    else if (!strcmp(argv[pn_index], "-y")) {
      fm_symb_merge_opt = false;
    }
    else {
      break;
    }
  }

  ITSModel * model;
  if (sogtype == FSLTL) {
    model = new fsltlModel();
  } else {
    model = new ITSModel();
  }

  vLabel nname = RdPELoader::loadModularProd(*model,pathprodff);
//  PNet * pnet = ProdLoader::loadProd(pathprodff);
//   model.declareType(*pnet);
//   modelName += pathprodff ;
   model->setInstance(nname,"main");
   model->setInstanceState("init");

  if (print_pn)
    std::cout << *model << std::endl;


//   // Parse and build the model !!!
//   loadTrains(2,model);
//   // Update the model to point at this model type as main instance
//   model.setInstance("Trains","main");
//   // The only state defined in the type "trains" is "init"
//   // This sets the initial state of the main instance
//   model.setInstanceState("init");

  // Initialize spot
  spot::ltl::parse_error_list pel;

  spot::ltl::formula* f = spot::ltl::parse(ltl_string, pel);
  if (spot::ltl::format_parse_errors(std::cerr, ltl_string, pel)) {
    f->destroy();
    return 1;
  }

  // Given the list of AP in the formula
  // Parse them one by one into model + error control if they don't exist
  // see example :
/*
std::string* check_at_prop(const petri_net* p,
                           const spot::ltl::formula* f,
                           spot::ltl::atomic_prop_set*& sap,
                           std::set<int>& ob_tr) {
  ob_tr.clear();
  sap = spot::ltl::atomic_prop_collect(f);

  if (sap) {
    spot::ltl::atomic_prop_set::iterator it;
    for(it = sap->begin(); it != sap->end(); ++it) {
      if(!p->place_exists( (*it)->name() )) {
        std::string* s = new std::string((*it)->name());
        delete sap;
        sap = 0;
        return s;
      }
      int pl = p->get_place_num((*it)->name());
      for (int t = 0; t < p->t_size(); ++t)
        if (p->get_incidence()[t].get(pl) != 0)
          ob_tr.insert(t);
    }
  }
  return 0;
}
*/


  /*
  if (print_rg)
    print_reachability_graph(n, place_marking_bound, f);

  if (count)
    count_markings(n, place_marking_bound, f);
  */



  if (check) {
    LTLChecker checker;
    checker.setFormula(f);
    checker.setModel(model);
    checker.setOptions(algo_string, ce_expected,
		       fm_exprop_opt, fm_symb_merge_opt,
		       post_branching, fair_loop_approx, "STATS", print_rg,
		       scc_optim, scc_optim_full, print_formula_tgba);
    checker.model_check(sogtype);
  }

  f->destroy();
  delete model;
  // external block for full garbage
  }
  MemoryManager::garbage();

  return 0;

}
