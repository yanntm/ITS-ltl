// Copyright (C) 2003, 2004, 2006, 2009 Laboratoire d'Informatique de
// Paris 6 (LIP6), d�partement Syst�mes R�partis Coop�ratifs (SRC),
// Universit� Pierre et Marie Curie.
//
// This file is part of Spot, a model checking library.
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

#include "slog.hh"
#include <string>
#include <cassert>
#include "misc/hashfunc.hh"

using namespace spot;


namespace slog
{

  ////////////////////////////////////////////////////////////
  // state_product
  slog_state::slog_state(const slog_state& o)
    : state(),
      left_(o.left()->clone()),
      right_(o.right())
  {
  }

  slog_state::~slog_state()
  {
    delete left_;
  }

  int
  slog_state::compare(const state* other) const
  {
    const slog_state* o = dynamic_cast<const slog_state*>(other);
    assert(o);
    int res = left_->compare(o->left());
    if (res != 0)
      return res;
    if (right_ == o->right())
      return 0;
    else
      return right_ < o->right() ? 1 : -1;
  }

  size_t
  slog_state::hash() const
  {
    // We assume that size_t is 32-bit wide.
    return wang32_hash(left_->hash()) ^ right_.hash();
  }

  slog_state*
  slog_state::clone() const
  {
    return new slog_state(*this);
  }

  ////////////////////////////////////////////////////////////
  // slog_succ_iterator

    /** aut : the automaton, passed to allow creation of iterators
     * left : the current succ iter on the autoamaton
     * model : the ITS model
     * right : the source aggregate */
  slog_succ_iterator::slog_succ_iterator(const tgba * aut, tgba_succ_iterator* left, const sogIts & model, const its::State& right)
    : aut_(aut),
      left_(left), 
      model_(model), 
      right_(right)
  {
  }

  slog_succ_iterator::~slog_succ_iterator()
  {
    delete left_;
  }

  bdd
  slog_succ_iterator::compute_weaker_selfloop_ap() 
  {
    // The acceptance condition labeling the arc of the tgba
    bdd ac = left_->current_acceptance_conditions();
    // The state reached in the tgba through this arc
    const state * q2 = left_->current_state();

    // Iterate over q2's successors, and add to F the atomic props which are on arcs "weaker" (wrt ac)
    bdd F = bddfalse;

    tgba_succ_iterator * it = aut_->succ_iter(q2);
    for ( it->first() ; ! it->done() ; it->next() ) {
      const state * dest = it->current_state();

      // Test self loop
      if ( dest->compare(q2) == 0 ) {	
	// Test ac=>ac' (subsume the arc)
	bdd acprime = it->current_acceptance_conditions(); 
	if ( (ac & acprime) == acprime) 
	  F |= it->current_condition();	
      }
      delete dest;
    }
    delete it;

    return F;
  }

  void
  slog_succ_iterator::step_()
  {
    // progress to "entry" states of succ in ITS model
    bdd curcond = left_->current_condition();
    dest_ = model_.succSatisfying ( right_, curcond );
    
    // test if we have empty initial states => we can avoid computing subsumed arcs etc...
    if (dest_.empty())
      return;

    // grab the current subsumed arc conditions
    bdd F = compute_weaker_selfloop_ap();
    
    // costly saturate the aggregate
    dest_ = model_.leastPreTestFixpoint (dest_, F);

  }

  void
  slog_succ_iterator::next_non_false_()
  {
    while (!left_->done())
      {
	step_();
	
	if (! dest_.empty() ) {
	  // reached an appropriate non empty successor
	  return ;
	}
	// (else)
	// progress in tgba
	left_->next();
      }
  }

  void
  slog_succ_iterator::first()
  {
    left_->first();
    
    next_non_false_();
  }

  void
  slog_succ_iterator::next()
  {
    // Progress in the tgba succs
    left_->next();
    // search for a non empty succ aggregate
    next_non_false_();
  }

  bool
  slog_succ_iterator::done() const
  {
    return left_->done();
  }


  slog_state*
  slog_succ_iterator::current_state() const
  {
    return new slog_state(left_->current_state(),
			  dest_);
  }

  bdd
  slog_succ_iterator::current_condition() const
  {
    return left_->current_condition();
  }

  bdd slog_succ_iterator::current_acceptance_conditions() const
  {
    return left_->current_acceptance_conditions();
  }

  ////////////////////////////////////////////////////////////
  // slog_tgba

  /// \brief Constructor.
  /// \param left The left automata in the product.
  /// \param right The ITS model.
  slog_tgba::slog_tgba(const spot::tgba* left, const sogIts & right)
    : dict_(left->get_dict()), left_(left), model_(right)
  {
    // register that we use the same bdd variables (dict) as the automaton.
    // these vars are unregistered in dtor
    dict_->register_all_variables_of(&left_, this);
  }
  
  slog_tgba::~slog_tgba()
  {
    dict_->unregister_all_my_variables(this);
  }
  
  state*
  slog_tgba::get_init_state() const
  {
    return new slog_state(left_->get_init_state(),
			  model_.getInitialState() );
  }

  slog_succ_iterator*
  slog_tgba::succ_iter(const state* local_state,
		       const state* global_state,
		       const tgba* global_automaton) const
  {
    const slog_state* s =
      dynamic_cast<const slog_state*>(local_state);
    assert(s);

    tgba_succ_iterator* li = left_->succ_iter(s->left(),
					      global_state, 
					      global_automaton);
  
    return new slog_succ_iterator(left_, li, model_, s->right());
  }

  bdd
  slog_tgba::compute_support_conditions(const state* in) const
  {
    const slog_state* s = dynamic_cast<const slog_state*>(in);
    assert(s);
    return left_->support_conditions(s->left());
  }

  bdd
  slog_tgba::compute_support_variables(const state* in) const
  {
    const slog_state* s = dynamic_cast<const slog_state*>(in);
    assert(s);
    return left_->support_variables(s->left());
  }

  bdd_dict*
  slog_tgba::get_dict() const
  {
    return dict_;
  }

  std::string
  slog_tgba::format_state(const state* state) const
  {
    const slog_state* s = dynamic_cast<const slog_state*>(state);
    assert(s);
    return (left_->format_state(s->left())
	    + " * "
	    + " SDD size: " + to_string(s->right().nbStates()) + " hash:" + to_string(s->right().hash()));
  }

  state*
  slog_tgba::project_state(const state* s, const tgba* t) const
  {
    const slog_state* s2 = dynamic_cast<const slog_state*>(s);
    assert(s2);
    if (t == this)
      return s2->clone();
    state* res = left_->project_state(s2->left(), t);
    if (res)
      return res;

    // TODO : to allow right projection, we need to encapsulate 
    // an aggregate of the ITS states inside a (dedicated?) spot::state
    return 0;
  }

  bdd
  slog_tgba::all_acceptance_conditions() const
  {
    return left_->all_acceptance_conditions();
  }

  bdd
  slog_tgba::neg_acceptance_conditions() const
  {
    return left_->neg_acceptance_conditions();
  }

  std::string
  slog_tgba::transition_annotation(const tgba_succ_iterator* t) const
  {
    const slog_succ_iterator* i =
      dynamic_cast<const slog_succ_iterator*>(t);
    assert(i);
    return left_->transition_annotation(i->left_);
  }

}
