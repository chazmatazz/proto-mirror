/* Proto optimizer Copyright (C) 2009, Jacob Beal, and contributors
listed in the AUTHORS file in the MIT Proto distribution's top directory.

This file is part of MIT Proto, and is distributed under the terms of
the GNU General Public License, with a linking exception, as described
in the file LICENSE in the MIT Proto distribution's top directory. */

// The analyzer takes us from an initial interpretation to a concrete,
// optimized structure that's ready for compilation

#include "analyzer.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "config.h"

#include "compiler.h"
#include "nicenames.h"

using namespace std;

#define DEBUG_FUNCTION(...) V5<<"In function " << __VA_ARGS__ << endl;

extern SE_Symbol *make_gensym(const string &root); // From interpreter

/*****************************************************************************
 *  TYPE CONCRETENESS                                                        *
 *****************************************************************************/

class Concreteness {
 public:
  // concreteness of fields comes from their types
  static bool acceptable(Field* f) { return acceptable(f->range); }
  // concreteness of types
  static bool acceptable(ProtoType* t) { 
    if(t->isA("ProtoNumber")) { return true;
    } else if(t->isA("ProtoSymbol")) { return true;
    } else if(t->isA("ProtoTuple")) { 
      ProtoTuple* tp = T_TYPE(t);
      for(int i=0;i<tp->types.size();i++) {
        if(!acceptable(tp->types[i])) {
          return false;
        }
      }
      return true;
    } else if(t->isA("ProtoLambda")) {
      return L_VAL(t)==NULL || acceptable(L_VAL(t));
    } else if(t->isA("ProtoField")) {
      return F_VAL(t)==NULL || acceptable(F_VAL(t));
    } else return false; // all others
  }
  // concreteness of operators, for ProtoLambda:
  static bool acceptable(Operator* op) { 
    if(op->isA("Literal") || op->isA("Parameter") || op->isA("Primitive")) {
      return true;
    } else if(op->isA("CompoundOp")) {
      return true; // its fields are checked elsewhere
    } else return false; // generic operator
  }
};

void CheckTypeConcreteness::act(Field* f) {
  if(!Concreteness::acceptable(f->range))
    //ierror(f,"Type is ambiguous: "+f->to_str());
    compile_error(f,"Type is ambiguous: "+f->to_str());
}

/*****************************************************************************
 *  TYPE CONSTRAINTS                                                         *
 *****************************************************************************/

SExpr* get_sexp(CE* src, string attribute) {
  Attribute* a = src->attributes[attribute];
  if(a==NULL || !a->isA("SExprAttribute"))
    return sexp_err(src,"Couldn't get expression for: "+a->to_str());
  return dynamic_cast<SExprAttribute &>(*a).exp;
}

/**
 * Return an unliteralized (maybe copy) of the type.
 * For example: deliteralize(<Scalar 2>) = <Scalar>.
 * Another example: deliteralize(<Scalar>) = <Scalar>.
 * The Deliteralization of a tuple is a tuple where each element has been
 * unliteralized.
 */
ProtoType* Deliteralization::deliteralize(ProtoType* base) {
  if(base->isA("ProtoVector")) {
    ProtoVector* t = &dynamic_cast<ProtoVector &>(*base);
    ProtoVector* newt = new ProtoVector(t->bounded);
    for(int i=0;i<t->types.size();i++)
      newt->types.push_back(deliteralize(t->types[i]));
    return newt;
  } else if(base->isA("ProtoTuple")) {
    ProtoTuple* t = &dynamic_cast<ProtoTuple &>(*base);
    ProtoTuple* newt = new ProtoTuple(t->bounded);
    for(int i=0;i<t->types.size();i++)
      newt->types.push_back(deliteralize(t->types[i]));
    return newt;
  } else if(base->isA("ProtoSymbol")) {
    ProtoSymbol* t = &dynamic_cast<ProtoSymbol &>(*base);
    return t->constant ? new ProtoSymbol() : t;
  } else if(base->isA("ProtoBoolean")) {
    ProtoBoolean* t = &dynamic_cast<ProtoBoolean &>(*base);
    return t->constant ? new ProtoBoolean() : t;
  } else if(base->isA("ProtoScalar")) {
    ProtoScalar* t = &dynamic_cast<ProtoScalar &>(*base);
    return t->constant ? new ProtoScalar() : t;
  } else if(base->isA("ProtoLambda")) {
    //TODO: This isnt the right place to do this
    //We want to propogate the output of a ProtoLambda, which would be the output value returned by the function
    //This isn't really "deliteralization" at all.
	ProtoLambda* t = &dynamic_cast<ProtoLambda &>(*base);
	return deliteralize(t->op->signature->output);
  } else if(base->isA("ProtoField")) {
	    ProtoField* t = &dynamic_cast<ProtoField &>(*base);
    return new ProtoField(deliteralize(t->hoodtype));
  } else {
    return base;
  }
}

  /********** TYPE READING **********/
  ProtoType* TypeConstraintApplicator::get_op_return(Operator* op) {
    DEBUG_FUNCTION(__FUNCTION__);
    if(!op->isA("CompoundOp"))
      return type_err(op,"'return' used on non-compound operator:"+ce2s(op));
    return dynamic_cast<CompoundOp &>(*op).output->range;
  }

  ProtoTuple* TypeConstraintApplicator::get_all_args(OperatorInstance* oi) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoTuple *t = new ProtoTuple();
    for(int i=0;i<oi->inputs.size();i++) t->add(oi->inputs[i]->range);
    return t;
  }
  
  ProtoType* TypeConstraintApplicator::get_nth_arg(OperatorInstance* oi, int n) {
    DEBUG_FUNCTION(__FUNCTION__);
    if(n < oi->op->signature->required_inputs.size()) { // ordinary argument
      if(n<oi->inputs.size()) return oi->inputs[n]->range;
      return type_err(oi,"Can't find type"+i2s(n)+" in "+ce2s(oi));
    } else if(n < oi->op->signature->n_fixed()) {
      if(n<oi->inputs.size()) return oi->inputs[n]->range;
      ierror("Nth arg calls not handling non-asserted optional arguments yet");
    } else if(n==oi->op->signature->n_fixed() && oi->op->signature->rest_input) {// rest
      //return a tuple of remaining elements
      vector<ProtoType*> vec;
      for(int i=n;i<oi->inputs.size();i++) vec.push_back(oi->inputs[i]->range);
      ProtoTuple *t = tupleOrVector(vec);
      V4 << "Returning a tuple for &rest: " << ce2s(t) << endl;
      return t;
    }
    return type_err(oi,"Can't find input "+i2s(n)+" in "+ce2s(oi));
  }

  /**
   * Gets a reference for 'last'.  Expects a tuple.
   * For example: (last <3-Tuple<Scalar 0>,<Scalar 1>,<Scalar 2>>) = <Scalar 2>
   */
  ProtoType* TypeConstraintApplicator::get_ref_last(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* nextType = get_ref(oi,li->get_next("type"));
    if(!nextType->isA("ProtoTuple")) {
      ierror(ref,"Expected ProtoTuple, but got "+ce2s(nextType)); // temporary, to help test suite
      return type_err(ref,"Expected ProtoTuple, but got "+ce2s(nextType));
    }
    ProtoTuple* tup = &dynamic_cast<ProtoTuple &>(*nextType);
    return tup->types[tup->types.size()-1];
  }
  
  /**
   * Takes the Lowest-common-superclass of its arguments.
   * Operates on proto types and rest elements.
   * For example: (lcs <Scalar> <Scalar 2> <3-Vector>) = <Number>
   */
  ProtoType* TypeConstraintApplicator::get_ref_lcs(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* nextType = NULL;
    ProtoType* compound = NULL;
    //for each arg to lcs, e.g., (lcs arg0 arg1)
    while(li->has_next()) {
      SExpr* next = li->get_next("type");
      nextType = get_ref(oi,next);
      if(isRestElement(oi, next) && nextType->isA("ProtoTuple")) {
        ProtoTuple* tv = &dynamic_cast<ProtoTuple &>(*nextType);
        for(int i=0;i<tv->types.size();i++) {
          compound=(compound==NULL)? tv->types[i] : ProtoType::lcs(compound, tv->types[i]);
          V4 << "get_ref (rest) lcs oi=" << ce2s(oi)
             << ", nextType=" << ce2s(nextType)
             << ", next[" << i << "]=" << ce2s(tv->types[i])
             << ", compound=" << ce2s(compound) <<endl;
        }
      } else {
        compound=(compound==NULL) ? nextType : ProtoType::lcs(compound, nextType);
        V4 << "get_ref lcs oi=" << ce2s(oi)
           << ", nextType=" << ce2s(nextType)
           << ", compound=" << ce2s(compound) << endl;
      }
    }
    return compound;
  }
  
  /**
   * Returns the nth type of a tuple.
   * For example: (nth <3-Tuple<Scalar 0>,<Scalar 1>,<Scalar 2>>, 1) = <Scalar 1>
   */
  ProtoType* TypeConstraintApplicator::get_ref_nth(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    //get tuple
    ProtoType* nextType = get_ref(oi,li->get_next("type"));
    ProtoTuple* tup = NULL;
    if(nextType->isA("ProtoTuple"))
       tup = &dynamic_cast<ProtoTuple &>(*nextType);

    //get index
    ProtoType* indexType;
    if(li->peek_next()->isScalar()) indexType = new ProtoScalar(li->get_num());
    else indexType = get_ref(oi,li->get_next("type"));
    ProtoScalar* index = NULL;
    if( indexType->isA("ProtoScalar") )
       index = &dynamic_cast<ProtoScalar &>(*indexType);

    //get result if possible
    if(index != NULL && tup != NULL && tup->types.size() > index->value) {
      V4<<"- tup[index] = " << ce2s(tup->types[index->value]) << endl;
      return tup->types[index->value];
    } else if (tup != NULL) {
      V4<<"- can't get the nth type yet... trying LCS"<< endl;
      ProtoType* ret = NULL;  
      if(tup->types.size() > 0)
        ret = tup->types[0];
      for(int i=1; i<tup->types.size() && ret != NULL; i++) {
        ret = ProtoType::lcs(ret,tup->types[i]);
      }
      if(ret == NULL)
        return new ProtoType();
      return ret;
    } 
    return new ProtoType();
  }

  /**
   * If all elements of tuple are <Scalar>, then it returns a ProtoVector,
   * otherwise it returns tuple.
   */
  ProtoTuple* TypeConstraintApplicator::tupleOrVector(vector<ProtoType*> types) {
    //TODO: I'm certain I can make this faster
    ProtoTuple* ret = NULL;
    bool allScalar = true;
    for(int i=0; allScalar && i<types.size(); i++) {
      //V4<<" ["<<i<<"] type = "<<ce2s(tuple->types[i])<<endl;
      if(!types[i]->isA("ProtoScalar"))
        allScalar = false;
    }
    if(allScalar) ret = new ProtoVector(true);
    else ret = new ProtoTuple(true);
    for(int i=0; i<types.size(); i++) {
      ret->add(types[i]);
    }
    return ret;
  }
  
  /**
   * Returns a tuple of its arguments or a vector if all arguments are scalars.
   * For example: (tupof <Scalar 1> <Tuple<Any>...>) = <2-Tuple <Scalar 1>,<Tuple<Any>...>>
   * For example: (tupof <Scalar 1> <Scalar 2>) = <2-Vector <Scalar 1>,<Scalar 2>>
   */
  ProtoType* TypeConstraintApplicator::get_ref_tupof(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    string tupstr = "get_ref tupof ";
    ProtoType* reftype = NULL;
    vector<ProtoType*> types;
    while( li->has_next() ) {
      SExpr* expr = li->get_next("type");
      reftype = get_ref(oi,expr);
      if(isRestElement(oi,expr) && reftype->isA("ProtoTuple")) {
        ProtoTuple* tup = T_TYPE(reftype);
        for(int i=0; i<tup->types.size(); i++) {
          tupstr += ce2s(tup->types[i]) + ", ";
          types.push_back(tup->types[i]);
        }
      } else {
        tupstr += ce2s(reftype) + ", ";
        types.push_back(reftype);
      }
    }
    V4 << tupstr << endl;
    return tupleOrVector(types);
  }
  
  /**
   * Returns a field of its arguments.
   * For example: (fieldof <Scalar 1>) = <Field <Scalar 1>>
   * Expects input to be a <Local>.  Otherwise returns the <Field> directly.
   */
  ProtoType* TypeConstraintApplicator::get_ref_fieldof(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* hoodtype = get_ref(oi,li->get_next("type"));
    return new ProtoField(hoodtype);
  }
  
  /**
   * Returns the deliteralized value of its argument.
   * For example: (unlit <Scalar 1>) = <Scalar>
   */
  ProtoType* TypeConstraintApplicator::get_ref_unlit(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    // get the argument
    ProtoType* reftype = get_ref(oi,li->get_next("type"));
    // can have at most 1 argument
    if(li->has_next()) {
      SExpr* unexpected = li->get_next("type");
      ierror(ref,"Unexpected extra argument "+ce2s(unexpected)); // temporary, to help test suite
      return type_err(ref,"Unexpected extra argument "+ce2s(unexpected));
    }
    V4<<"get_ref unlit " << ce2s(reftype) <<endl;
    return Deliteralization::deliteralize(reftype);
  }
  
  /**
   * Returns the field-type of its input.
   * For example: (ft <Field <Scalar 1>>) = <Scalar 1>
   * Expects a field as input. 
   * Can coerce a ProtoField from a ProtoLocal e.g.,
   * (ft <Scalar 1>) = <Scalar 1>
   */
  ProtoType* TypeConstraintApplicator::get_ref_ft(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* reftype = get_ref(oi,li->get_next("type"));
    if(!reftype->isA("ProtoField")) {
      if(reftype->isA("ProtoLocal")) {
        V3<<"Coercing a ProtoField for {"<<ce2s(ref)<<"} of {"<<ce2s(oi)<<"}"<<endl;
        return reftype;
      } else if(reftype->type_of()=="ProtoType") { // top-level type
        return reftype;
      }
      // should never get here
      ierror(ref,"Unhandled ProtoField ref case: "+ce2s(reftype)); // temporary, to help test suite
    }
    ProtoField* field = &dynamic_cast<ProtoField &>(*reftype);
    return field->hoodtype;
  }

  /**
   * Returns the input types of its argument's signature.
   * For example: 
   *   (def foo (scalar vector) number)
   *   (inputs <Lambda foo>) = <2-Tuple <Scalar>,<Vector>>
   */
  ProtoType* TypeConstraintApplicator::get_ref_inputs(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* reftype = get_ref(oi,li->get_next("type"));
    if(!reftype->isA("ProtoLambda")) { 
      ierror(ref,"Expected ProtoLambda, but got "+ce2s(reftype)); // temporary, to help test suite
      return type_err(ref,"Expected ProtoLambda, but got "+ce2s(reftype));
    }
    ProtoLambda* lambda = &dynamic_cast<ProtoLambda &>(*reftype);
    Signature* sig = lambda->op->signature;
    // we only know the length of the inputs if there's no &rest
    ProtoTuple* ret = new ProtoTuple(!sig->rest_input);
    for(int i=0; i<sig->n_fixed(); i++) {
      ProtoType* elt = ProtoType::clone(sig->nth_type(i));
      ret->add(elt);
    }
    if(sig->rest_input!=NULL) // rest is unbounded end to tuple
      ret->add(sig->rest_input);
    return ret;
  }

  /**
   * Returns the input types of its argument's signature.
   * For example: 
   *   (def foo (scalar vector) number)
   *   (output <Lambda foo>) = <Number>
   */
  ProtoType* TypeConstraintApplicator::get_ref_output(OperatorInstance* oi, SExpr* ref, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* reftype = get_ref(oi,li->get_next("type"));
    if(!reftype->isA("ProtoLambda")) { 
      ierror(ref,"Expected ProtoLambda, but got "+ce2s(reftype)); // temporary, to help test suite
      return type_err(ref,"Expected ProtoLambda, but got "+ce2s(reftype));
    }
    ProtoLambda* lambda = &dynamic_cast<ProtoLambda &>(*reftype);
    return lambda->op->signature->output;
  }

  /**
   * Gets the reference type for any 'list' type (any non-symbol)
   */
  ProtoType* TypeConstraintApplicator::get_ref_list(OperatorInstance* oi, SExpr* ref) {
    DEBUG_FUNCTION(__FUNCTION__);
    SE_List_iter* li = new SE_List_iter(ref);
    // "fieldof": field containing a local
    if(li->on_token("fieldof"))
       return get_ref_fieldof(oi,ref,li);
    // "ft": type of local contained by a field
    if(li->on_token("ft"))
       return get_ref_ft(oi,ref,li);
    // "inputs": the types of a function's input signature
    if(li->on_token("inputs"))
       return get_ref_inputs(oi,ref,li);
    // "last": find the last type in a tuple
    if(li->on_token("last"))
       return get_ref_last(oi,ref,li);
    // "lcs": finds the least common superclass of a set of types
    if(li->on_token("lcs")) 
       return get_ref_lcs(oi,ref,li);
    // "nth": finds the nth type in a tuple
    if(li->on_token("nth")) 
       return get_ref_nth(oi,ref,li);
    // "outputs": the type of a function's output
    if(li->on_token("output"))
       return get_ref_output(oi,ref,li);
    // "tupof": tuple w. a set of types as arguments
    if(li->on_token("tupof")) 
       return get_ref_tupof(oi,ref,li);
    // "unlit": generalize away literal values
    if(li->on_token("unlit")) 
       return get_ref_unlit(oi,ref,li);
  }
  
  /**
   * Return the best available information on the referred type
   */
  ProtoType* TypeConstraintApplicator::get_ref(OperatorInstance* oi, SExpr* ref) {
    V4<<"=========================="<< endl;
    DEBUG_FUNCTION(__FUNCTION__);
    V4<<"get_ref on " << ce2s(ref) << " from " << ce2s(oi) << endl;
    V4<<"Getting type reference for "<<ce2s(ref)<<endl;
    if(ref->isSymbol()) {
      // Named input/output argument:
      int n = oi->op->signature->parameter_id(&dynamic_cast<SE_Symbol &>(*ref),
          false);
      V4 << "n : " << n << endl;
      if(n>=0) {
        ProtoType* ret = get_nth_arg(oi,n);
        if (oi->attributes.count("LETFED-MUX")) {
          if (ret->isA("ProtoLambda")) {
            ProtoLambda* lambda = &dynamic_cast<ProtoLambda &>(*ret);
        	if (lambda->op->isA("CompoundOp")) {
    	      ProtoType* rtype = dynamic_cast<CompoundOp &>(*lambda->op).output->range;
    		  ret = rtype;
    	    }
          }
        }
    	V4 << "nth arg: " << ce2s(ret) << endl;
    	return ret;
      }
      if(n==-1) {
    	  V4 << "oi->output->range " << ce2s(oi->output->range) << endl;
    	  ProtoType *ret = oi->output->range;
    	  if(oi->attributes.count("LETFED-MUX")) {
    		if (oi->output->range->isA("ProtoLambda")) {
    	      ProtoLambda* lambda = &dynamic_cast<ProtoLambda &>(*oi->output->range);
    	      if (lambda->op->isA("CompoundOp")) {
    	        ProtoType* rtype = dynamic_cast<CompoundOp &>(*lambda->op).output->range;
    	  	    ret = rtype;
    	      }
    	    }
    	  }
    	  return ret;
      }
      // "args": a tuple of argument types
      if(*ref=="args") return get_all_args(oi);
      // return: the value of a compound operator's output field
      if(*ref=="return") return get_op_return(oi->op);
    } else if(ref->isList()) {
      ProtoType* ret = get_ref_list(oi,ref);
      V4<<"get_ref returns: " << ce2s(ret) << endl;
      return ret;
    }
    ierror(ref,"Unknown type reference: "+ce2s(ref)); // temporary, to help test suite
    return type_err(ref,"Unknown type reference: "+ce2s(ref));
  }

/********** TYPE ASSERTION **********/
bool TypeConstraintApplicator::assert_range(Field* f,ProtoType* range) {
  // Assertion only narrows types, it does not widen them:
  if(f->range->supertype_of(range)) { return parent->maybe_set_range(f,range); }
  else { return false; }
}

  /**
   * Returns true if n is the &rest element of oi
   */
  bool isNthArgRest(OI* oi, int n) {
    return (n==oi->op->signature->n_fixed() && oi->op->signature->rest_input);
  }
  
  /**
   * Asserts that the nth argument of oi is value.
   */
  bool TypeConstraintApplicator::assert_nth_arg(OperatorInstance* oi, int n, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    if(n >= 0 && n <= oi->inputs.size()) {
      if(isNthArgRest(oi,n) && value->isA("ProtoTuple")) {
        ProtoTuple* inputTup = new ProtoTuple(true);
        // add all the inputs to a tuple
        for(int i=oi->op->signature->n_fixed(); i<oi->inputs.size(); i++) {
          inputTup->add(oi->inputs[i]->range);
        }
        V4<<"assert_nth_arg (rest): oi["<<n<<"]= "<<ce2s(inputTup)<<", val="<<ce2s(value)<<endl;
        // for all the inputs, change their type if necessary
        ProtoTuple* valueTup = T_TYPE(value);
        bool retVal = false;
        for(int i=0; i<inputTup->types.size(); i++) {
           ProtoType* valType = NULL;
           // if valueTup is unbounded, use the last element for all remaining
           if(i>=valueTup->types.size()) {
             valType = valueTup->types[valueTup->types.size()-1];
           } else {
             valType = valueTup->types[i];
           }
           int which_input = i+oi->op->signature->n_fixed();
           retVal |= assert_range(oi->inputs[which_input],valType);
        }
        return retVal;
      }
      V4<<"assert_nth_arg: oi["<<n<<"]= "<<ce2s(oi->inputs[n]->range)<<", val="<<ce2s(value)<<endl;
      if(n<oi->inputs.size())
        return assert_range(oi->inputs[n],value);
    }
    ierror("Could not find argument "+i2s(n)+" of "+ce2s(oi));
  }

  /**
   * Asserts that all the arguments of oi are value.
   * @WARNING not implemented
   */
  bool TypeConstraintApplicator::assert_all_args(OperatorInstance* oi, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    ierror("Not yet implemented");
    return false;
  }

  /**
   * Asserts that output oi is value.
   * @WARNING not implemented
   */
  bool TypeConstraintApplicator::assert_op_return(Operator* f, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    ierror("Not yet implemented");
    return false;
  }
  
  /**
   * Asserts that (fieldof ref) = value.
   * Expects ref to resolve to a ProtoLocal.
   */
  bool TypeConstraintApplicator::assert_on_field(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    if(!value->isA("ProtoField"))
      ierror("'fieldof' assertion on non-field type: "+ref->to_str());
    return assert_ref(oi,ref,F_VAL(value));
  }
  
  /**
   * Asserts that (tupof ref) = value.
   * Expects ref to resolve to a ProtoLocal.
   */
  bool TypeConstraintApplicator::assert_on_tup(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    if(!value->isA("ProtoTuple"))
      ierror("'tupof' assertion on non-tuple type: "+ref->to_str());
    return assert_ref(oi,ref,T_TYPE(value));
  }
  
  /**
   * Asserts that (ft ref) = value.
   * Expects ref to resolve to a ProtoField.
   */
  bool TypeConstraintApplicator::assert_on_ft(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    if(!value->isA("ProtoField")) {
      if(value->isA("ProtoLocal")) {
        V4 << "coercing a ProtoLocal into a ProtoField" << endl;
        return assert_ref(oi,ref,new ProtoField(value));
      }
      ierror("'ft' assertion on non-field type: "+ref->to_str());
    }
    return assert_ref(oi,ref, F_TYPE(value));
  }
  
  /**
   * Asserts that (output ref) = value.
   * Expects ref to resolve to a ProtoLambda.
   */
  bool TypeConstraintApplicator::assert_on_output(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* lambda_arg = get_ref(oi,ref);
    if(!lambda_arg->isA("ProtoLambda"))
       ierror("'output' assertion on a non-lambda type: "+ref->to_str()+" (it's a "+lambda_arg->to_str()+")");
    ProtoLambda* lambda = &dynamic_cast<ProtoLambda &>(*lambda_arg);
    //Signature* sig = lambda->op->signature;
    return assert_ref(oi,ref,lambda);
  }
  
  /**
   * Asserts that (inputs ref) = value.
   * Expects ref to resolve to a ProtoLambda.
   * Expects value to be a ProtoTuple.
   */
  bool TypeConstraintApplicator::assert_on_inputs(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* lambda_arg = get_ref(oi,ref);
    if(!lambda_arg->isA("ProtoLambda"))
       ierror("'inputs' assertion on a non-lambda type: "+ref->to_str()+" (it's a "+lambda_arg->to_str()+")");
    ProtoLambda* lambda = &dynamic_cast<ProtoLambda &>(*lambda_arg);
    Signature* sig = lambda->op->signature;
    if(!value->isA("ProtoTuple"))
       ierror("'inputs' assertion on a non-tuple type: value (it's a "+value->to_str()+")");
    ProtoTuple* tup = T_TYPE(value);
    for(int i=0;i<sig->n_fixed()&&i<tup->types.size();i++) {
      sig->set_nth_type(i, ProtoType::gcs(sig->nth_type(i),tup->types[i]));
    }
    /* TODO: Not sure how to do REST elements yet
    if(sig->rest_input) {
       ProtoType* gcs = 
          ProtoType::gcs(sig->rest_input,tup->types[tup->types.size()-1]);
       if(gcs)
         sig->rest_input = gcs;
    }
    */
    return assert_ref(oi,ref,lambda);
  }
  
  /**
   * Asserts that (last ref) = value.
   * Expects ref to resolve to a ProtoTuple.
   */
  bool TypeConstraintApplicator::assert_on_last(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* argtype = get_ref(oi, ref);
    ProtoTuple* tup = NULL;
    if(!argtype->isA("ProtoTuple")) {
      V3<<"Coercing "<<ce2s(argtype)<<" to ProtoTuple"<<endl;
      tup = new ProtoTuple(); tup->add(value);
    } else {
      // clone and replace last element
      tup = new ProtoTuple(T_TYPE(argtype));
      if(tup->bounded) {
        tup->types[tup->types.size()-1] = value;
      } else {
        // can't establish the last value of an unbounded tuple
      }
    }
    return assert_ref(oi,ref,tup);
  }

  /**
   * Gets the LCS of nextType and prevType.  isRestElem must be true if
   * nextType is a ProtoTuple derived from a &rest element in its function
   * signature.
   */
  ProtoType* TypeConstraintApplicator::getLCS(ProtoType* nextType, ProtoType* prevType, bool isRestElem) {
    ProtoType* compound = prevType;
    // if it's a &rest element, take the LCS of each sub-argument
    if(isRestElem && nextType->isA("ProtoTuple")) {
      ProtoTuple* tv = &dynamic_cast<ProtoTuple &>(*nextType);
      for(int i=0;i<tv->types.size();i++) {
        compound=(compound==NULL)? tv->types[i] : ProtoType::lcs(compound, tv->types[i]);
        V4 << "Assert (rest) lcs: next=" << ce2s(nextType)
           << ", next[" << i << "]=" << ce2s(tv->types[i])
           << ", compound=" << ce2s(compound) <<endl;
      }
    }
    // if it's not a &rest, take the LCS of each argument
    else {
      compound=(compound==NULL)? nextType : ProtoType::lcs(compound, nextType);
      V4 << "Assert lcs: next=" << ce2s(nextType)
         << ", compound=" << ce2s(compound) <<endl;
    }
    return compound;
  }
  
  /**
   * Asserts that (lcs ref) = value.
   * Reads the remainder of arguments from li and takes the LCS of all the arguments in ref.
   */
  bool TypeConstraintApplicator::assert_on_lcs(OperatorInstance* oi, SExpr* ref, ProtoType* value, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    SExpr* next = NULL;
    // get the first arg to LCS
    V4 << "assert LCS ref:   " << ce2s(ref) << endl;
    V4 << "assert LCS value: " << ce2s(value) << endl;
    ProtoType* compound = getLCS(value,NULL,isRestElement(oi,ref));
    bool ret = assert_ref(oi,ref,compound);
    // get the remaining args to LCS
    while(li->has_next()) {
      next = li->get_next("type");
      compound = getLCS(value,compound,isRestElement(oi,next));
      ret |= assert_ref(oi,next,compound);
      V4 << "assert LCS ref:   " << ce2s(next) << endl;
      V4 << "assert LCS value: " << ce2s(value) << endl;
    }
    return ret;
  }

  /**
   * Adds items to an unbounded 'tup' to ensure that it has at least
   * 'n' elements.  Return true if the size changed.
   */
  bool TypeConstraintApplicator::fillTuple(ProtoTuple* tup, int n) {
    if(tup->bounded) ierror("Cannot fill a bounded ProtoTuple: "+ce2s(tup));
    bool changed = false;
    for(int i=tup->types.size()-1; i<n; i++) {
      ProtoType* elt = ProtoType::clone(tup->types[i]); // copy rest element
      tup->add(elt); // add to end of tuple
      changed = true;
    }
    return changed;
  }
  
  /**
   * Asserts that (nth next ...li...) = value.
   * Expects next to be a ProtoTuple and reads the next type from li, which it
   * expects to be a ProtoScalar.  Uses the ProtoScalar as the index into the
   * ProtoTuple.  Asserts that the nth item of the tuple is value.
   */
  bool TypeConstraintApplicator::assert_on_nth(OperatorInstance* oi, SExpr* next, ProtoType* value, SE_List_iter* li) {
    DEBUG_FUNCTION(__FUNCTION__);
    //1) ---tuple---
    //ProtoType* tupType = ProtoType::gcs(get_ref(oi,next),new ProtoTuple());
    ProtoType* tupType = get_ref(oi,next);
    // ensure that <Any> get treated as ProtoTuple
    ProtoType* gcsTup = ProtoType::gcs(tupType, new ProtoTuple());
    if(gcsTup == NULL || !gcsTup->isA("ProtoTuple")) {
       ierror("'nth' assertion on a non-tuple type: "+next->to_str()
              +" (it's a "+tupType->to_str()+")");
       return false;
    }
    //we can at least say that the first argument is a <Tuple <Any>...>
    bool changedTup = assert_ref(oi,next,T_TYPE(gcsTup)); 

    //2) ---scalar---
    SExpr* indexExpr = li->get_next("type");
    ProtoType* indexType;
    bool changedScalar = false;
    if(indexExpr->isScalar()) {
      indexType = new ProtoScalar(dynamic_cast<SE_Scalar &>(*indexExpr).value);
      V4 << "nth assertion found constant index: " << ce2s(indexType) << endl;
    } else {
      indexType = ProtoType::gcs(get_ref(oi,indexExpr),new ProtoScalar());
      V4 << "nth assertion found var index type: " << ce2s(indexType) << endl;
      //we can at least say that the second arg is a <Scalar>
      changedScalar = assert_ref(oi,indexExpr,S_TYPE(indexType));
    }
    if(indexType == NULL || !indexType->isA("ProtoScalar")) {
       ierror("'nth' assertion on a non-scalar type: "+indexExpr->to_str()+" (it's a "+indexType->to_str()+")");
    }
    
    //3) ---nth of tup---
    tupType = get_ref(oi,next); // re-get the tuple-ized tuple
    V4 << "asserting nth element "<<ce2s(indexType)<<" as "<<ce2s(value)
       <<" on "<<ce2s(tupType)<<endl;
    ProtoTuple* newt = new ProtoTuple(T_TYPE(tupType));
    if(!tupType->isA("ProtoTuple"))
       ierror("'nth' assertion failed to coerce a tuple from type: "+next->to_str()
              +" (it's a "+tupType->to_str()+")");
    if(indexType->isLiteral() //we know the index & it's valid
              && (int)S_VAL(indexType) >= 0) {
      int i = S_VAL(indexType);
      if(newt->bounded) {
        if(i >= newt->types.size())
           ierror("'nth' assertion index ("+indexType->to_str()+
                  ") exceeded tuple length of bounded Tuple: "+ce2s(tupType));
        newt->types[i] = value;
      } else {
        int min_size_of_tup = i+1;
        fillTuple(newt,min_size_of_tup);
        newt->types[i] = value;
      }
    }
    bool changedNth = assert_ref(oi,next,newt);
    return changedTup || changedScalar || changedNth;
  }
  
  /**
   * Asserts that (unlit ref) = value.
   * Asserts a de-literalized value onto its arguments.
   */
  bool TypeConstraintApplicator::assert_on_unlit(OperatorInstance* oi, SExpr* next, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    return assert_ref(oi,next,Deliteralization::deliteralize(value));
  }

  /**
   * Returns true iff type is a Tuple derived from a &rest element.
   */
  bool TypeConstraintApplicator::isRestElement(OperatorInstance* oi, SExpr* ref) {
    if(!ref->isSymbol()) return false;
    Signature* s = oi->op->signature;
    return s->rest_input
      && (s->parameter_id(&dynamic_cast<SE_Symbol &>(*ref)) == s->n_fixed());
  }

  /**
   * Asserts value onto a list (ref) from oi.
   */
  bool TypeConstraintApplicator::assert_ref_list(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    DEBUG_FUNCTION(__FUNCTION__);
    SE_List_iter li(ref);
    // "fieldof": field containing a local
    if(li.on_token("fieldof")) 
      return assert_on_field(oi,li.get_next("type"),value);
    // "ft": type of local contained by a field
    if(li.on_token("ft")) 
      return assert_on_ft(oi,li.get_next("type"),value);
    // "inputs": the types of a function's input signature
    if(li.on_token("inputs")) 
      return assert_on_inputs(oi,li.get_next("type"),value);
    // "last": find the last type in a tuple
    if(li.on_token("last")) 
       return assert_on_last(oi,li.get_next("type"),value);
    // "lcs": finds the least common superclass of a set of types
    if(li.on_token("lcs")) 
       return assert_on_lcs(oi,li.get_next("type"),value,&li);
    // "nth": finds the nth type in a tuple
    if(li.on_token("nth")) 
       return assert_on_nth(oi,li.get_next("type"),value,&li);
    // "outputs": the type of a function's output
    if(li.on_token("output"))
      return assert_on_output(oi,li.get_next("type"),value);
    // "tupof": tuple w. a set of types as arguments
    if(li.on_token("tupof"))
      return assert_on_tup(oi,li.get_next("type"),value);
    // "unlit": generalize away literal values
    if(li.on_token("unlit")) 
      return assert_on_unlit(oi,li.get_next("type"),value);
  }

  /** 
   * Core assert dispatch function.
   * If possible, modify the referred type with a GCS.
   */
  bool TypeConstraintApplicator::assert_ref(OperatorInstance* oi, SExpr* ref, ProtoType* value) {
    V4<<"=========================="<< endl;
    DEBUG_FUNCTION(__FUNCTION__);
    V2<<"assert_ref "<< ce2s(ref) << " as " << ce2s(value) << endl;
    V4<<"Asserting type "<<ce2s(value)<<" for reference "<<ce2s(ref)<<endl;
    if(ref->isSymbol()) {
      // Named input/output argument:
      int n = oi->op->signature->parameter_id(&dynamic_cast<SE_Symbol &>(*ref),
          false);
      if(n>=0) return assert_nth_arg(oi,n,value);
      if(n==-1) return assert_range(oi->output,value);
      // "args": a tuple of argument types
      if(*ref=="args") return assert_all_args(oi,value);
      // return: the value of a compound operator's output field
      if(*ref=="return") return assert_op_return(oi->op,value);
    } else if(ref->isList()) {
      return assert_ref_list(oi,ref,value);
    }
    ierror(ref,"Unknown type reference: "+ce2s(ref)); // temporary, to help test suite
    return type_err(ref,"Unknown type reference: "+ce2s(ref));
  }

  bool TypeConstraintApplicator::repair_field_constraint(OI* oi, 
                                                         ProtoField* field,
                                                         ProtoType* local) {
    ProtoType* newt = ProtoType::gcs(field,new ProtoField(local));
    V4 << "Repair changing " << ce2s(local) << " to " << ce2s(newt) << endl;
    local = newt;
    if(oi->pointwise()==1) {
      //Fieldify
      Operator* fo = FieldOp::get_field_op(oi);
      if(fo) {
        V4 << "Repair changing op"<<ce2s(oi->op)<<" to "<<ce2s(fo)<<endl;
        oi->op = fo;
      }
    }
    if(!oi->output->range->isA("ProtoField")) {
      //Change output
      ProtoType* newt = new ProtoField(oi->output->range);
      V4 << "Repair changing output " << ce2s(oi->output->range) 
         << " to " << ce2s(newt) << endl;
      oi->output->range = newt;
    }
    return true;
  }

// TODO: this code and the other field repair code ought to be merged, somehow
  bool TypeConstraintApplicator::repair_constraint_failure(OI* oi, 
                                                           ProtoType* ftype, 
                                                           ProtoType* ctype) {
    if(ftype->isA("ProtoField") && ctype->isA("ProtoLocal")) {
      return repair_field_constraint(oi, F_TYPE(ftype), ctype);
    } else if(ctype->isA("ProtoField") && ftype->isA("ProtoLocal")) {
      return repair_field_constraint(oi, F_TYPE(ctype), ftype);
    }
    if(oi->op==Env::core_op("local") && ftype->isA("ProtoField")) {
      return true; // defer repair to later
    }
    return false;
  }
  
  /********** External Interface **********/
  bool TypeConstraintApplicator::apply_constraint(OperatorInstance* oi, SExpr* constraint) {
    DEBUG_FUNCTION(__FUNCTION__);
    SE_List_iter li(constraint,"type constraint");
    if(!li.has_next())
      { compile_error(constraint,"Empty type constraint"); return false; }
    if(li.on_token("=")) { // types should be identical
      V4<<"type constraint: "+ce2s(li.peek_next())<<endl;
      SExpr *aref = li.get_next("type reference");
      SExpr *bref=li.get_next("type reference");
      ProtoType *a = get_ref(oi,aref), *b = get_ref(oi,bref);
      V3<<"apply_constraint: aref("<<ce2s(aref)<<")="<<ce2s(a)
         <<", bref("<<ce2s(bref)<<")="<<ce2s(b) << endl;
      // first, take the GCS
      ProtoType *joint = ProtoType::gcs(a,b);
      if(joint==NULL) { // if GCS shows conflict, attempt to correct
        if(verbosity > 4)
          parent->root->printdot(cpout);
        if(!repair_constraint_failure(oi,a,b))
          type_err(oi,"Type constraint "+ce2s(constraint)+" violated: \n  "
                   +a->to_str()+" vs. "+b->to_str()+" at "+ce2s(oi));
        return true; // note that a change has occurred
      } else { // if GCS succeeded, assert onto referred locations
        V3<<"apply_constraint: joint="<<ce2s(joint)<< endl;
        bool ret = assert_ref(oi,aref,joint) | assert_ref(oi,bref,joint);
        V4<<"apply_constraint: done"<< endl;
        if(verbosity > 4)
          parent->root->printdot(cpout);
        return ret;
      }
      // if it's OK, then push back:  maybe_set_ref(oi,constraint[i]);
      // if it's not OK, then call for resolution
    } else {
      compile_error("Unknown constraint '"+ce2s(li.peek_next())+"'");
    }
    return false;
  }
  
  bool TypeConstraintApplicator::apply_constraints(OperatorInstance* oi, SExpr* constraints) {
    DEBUG_FUNCTION(__FUNCTION__);
    bool changed=false;
    SE_List_iter li(constraints,"type constraint set");
    while(li.has_next()) 
      { changed |= apply_constraint(oi,li.get_next("type constraint")); }
    return changed;
  }

/*****************************************************************************
 *  TYPE RESOLUTION                                                          *
 *****************************************************************************/

// Type resolution needs to be applied to anything that isn't
// concrete...

class TypePropagator : public IRPropagator {
 public:
  TypePropagator(DFGTransformer* parent, Args* args)
    : IRPropagator(true,true,true) {
    verbosity = args->extract_switch("--type-propagator-verbosity") ? 
      args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream* out=0) { *out << "TypePropagator"; }
  
  // implicit type conversion or other modification to fix conflict
  // returns true if repair is successful
  bool repair_conflict(Field* f,Consumer c,ProtoType* ftype,ProtoType* ctype) {
   
   	if (c.first != NULL) {
		OI *cfirst = c.first;
        if(cfirst->op == Env::core_op("mux") &&
           cfirst->inputs.size() >= 3 &&
           cfirst->domain() != cfirst->inputs[1]->domain &&
           cfirst->domain() != cfirst->inputs[2]->domain ) {
          // Compilation error if a field crosses OUT of an AM boundary
          compile_error("Cannot restrict (if) inside a neighborhood operation ("+ce2s(c.first->op)+")");
        }
	}
	
    // if vector is needed and scalar is provided, convert to a size-1 vector
    if(ftype->isA("ProtoScalar") && ctype->isA("ProtoVector")) {
      V4<<"repair scalar->vector"<< endl;
      if(c.first==NULL) return true; // let it be repaired in Field action stage
      V2<<"Converting Scalar to 1-Vector\n";
      OI* tup = new OperatorInstance(f->producer,Env::core_op("tup"),f->domain);
      tup->add_input(f);
      root->relocate_source(c.first,c.second,tup->output);
      note_change(tup); return true; // let constraint retry later...
    }
    
    // if source is local and user wants a field, add a "local" op
    // or replace no-argument source with a field op
    if(ftype->isA("ProtoLocal") && ctype->isA("ProtoField")) {
      V4<<"repair local->field "<< ce2s(ftype) << " " << ce2s(ctype) << endl;
      V4 << "f->producer: " << ce2s(f->producer) << endl;
      V4 << "f->producer-op: " << ce2s(f->producer->op) << endl;
      if(c.first==NULL) return true; // let it be repaired in Field action stage
      ProtoField* ft = &dynamic_cast<ProtoField &>(*ctype);
      if(ProtoType::gcs(ftype,ft->hoodtype)) {
        Operator* fo = FieldOp::get_field_op(f->producer);

        if(f->producer->op == Env::core_op("mux") &&
                          f->producer->inputs.size() >= 3 &&
                          f->producer->domain() != f->producer->inputs[1]->domain &&
                          f->producer->domain() != f->producer->inputs[2]->domain ) {
                  // Compilation error if a field crosses OUT of an AM boundary
                  compile_error("Cannot restrict (if) inside a neighborhood operation ("+ce2s(c.first->op)+")");
        } else if (f->producer->inputs.size()==0 && f->producer->pointwise()!=0 && fo) {
          V2<<"Fieldify pointwise: "<<ce2s(f->producer)<<endl;
          OI* foi = new OperatorInstance(f->producer,fo,f->domain);
          root->relocate_source(c.first,c.second,foi->output);
          note_change(foi); return true; // let constraint retry later...
        } else {

          // Otherwise, insert a local
          V2<<"Inserting 'local' at "<<ce2s(f)<<endl;
          OI* local = new OI(f->producer,Env::core_op("local"),f->domain);
          local->add_input(f);
          root->relocate_source(c.first,c.second,local->output);
          note_change(local); return true; // let constraint retry later...
        }
      }
    }
    
    // if source is field and user is "local", send to the local's consumers
    if(ftype->isA("ProtoField") && c.first&&c.first->op==Env::core_op("local")){
      V4<<"repair field->local"<< endl;
      if(c.first==NULL) return true; // let it be repaired in Field action stage
      V2<<"Bypassing 'local' at "<<ce2s(f)<<endl;
      root->relocate_consumers(c.first->output,f);
      return true;
    }
    
    // if source is field and user is pointwise, upgrade to field op
    if(ftype->isA("ProtoField") && ctype->isA("ProtoLocal")) {
      V4<<"repair field->pointwise "<< c.first << endl;
      if(c.first==NULL) return true; // let it be repaired in Field action stage
      ProtoField* ft = &dynamic_cast<ProtoField &>(*ftype);
      int pw = c.first->pointwise();
      V4<<"pointwise? "<< pw << endl;
      if(f->producer->op == Env::core_op("mux") &&
         f->producer->inputs.size() >= 3 &&
         f->producer->domain() != f->producer->inputs[1]->domain &&
         f->producer->domain() != f->producer->inputs[2]->domain ) {
         // Compilation error if a field crosses OUT of an AM boundary
         compile_error("Cannot restrict (if) inside a neighborhood operation ("+ce2s(c.first->op)+")");
      }
      if(pw!=0 && ProtoType::gcs(ctype,ft->hoodtype)) {
        Operator* fo = FieldOp::get_field_op(c.first); // might be upgradable
        V4<<"fieldop = "<< ce2s(fo) << endl;
        if(fo) {
          V2<<"Fieldify pointwise: "<<ce2s(c.first->op->signature)<<" to "<<ce2s(fo->signature)<<endl;
          c.first->op = fo; 
          note_change(c.first);
        }
        return true; // in any case, don't fail out now
      }
    }

    V4<<"no matching repairs"<< endl;
    return false; // repair has failed
  }

  /// apply a consumer constraint, managing implicit type conversions 
  bool back_constraint(ProtoType** tmp,Field* f,pair<OperatorInstance*,int> c) {
    DEBUG_FUNCTION(__FUNCTION__);
    ProtoType* ct = c.first->op->signature->nth_type(c.second);
    V3<<"Back constraint on: "<<ce2s(c.first)<<", input "<<i2s(c.second)<<endl;
    V3<<"- back type: "<<ce2s(*tmp)<<" vs. "<<ce2s(ct)<<"..."<<endl;
    // Attempt to narrow the type:
    ProtoType* newtype = ProtoType::gcs(*tmp,ct);
    if(newtype) { V3<<"- ok\n"; *tmp = newtype; return true; }
    else V3<<"- FAIL\n";

    // On merge failure, attempt to repair conflict
    if(repair_conflict(f,c,*tmp,ct)) return false;
    // having fallen through all cases, throw type error
    type_err(f,"conflict: "+f->to_str()+" vs. "+c.first->to_str());
    return false;
  }

  void act(Field* f) {
    V3 << "Considering field "<<ce2s(f)<<endl;
    // Ignore old type (it may change) [except Parameter]; use producer type
    ProtoType* tmp = f->producer->op->signature->output;
    if(f->producer->op->isA("Parameter")) {
      Parameter* p = (Parameter*)f->producer->op;
      tmp=ProtoType::gcs(f->range,p->container->signature->nth_type(p->index));
      V4<<"Parameter: "<<p->to_str()<<" range "<<f->range->to_str()<<" sigtype "
        <<p->container->signature->nth_type(p->index)->to_str()<<endl;
    }
    // GCS against current value
    ProtoType* newtmp = ProtoType::gcs(tmp,f->range);
    if(!newtmp) {
      V3 << "Attempting to repair conflict with producer "<<ce2s(tmp)<<endl;
      if(!repair_conflict(f, make_pair(static_cast<OI *>(0), -1), tmp,
          f->range)) {
        type_err(f,"incompatible type and signature "+f->to_str()+": "+
                 ce2s(tmp)+" vs. "+ce2s(f->range));
        return;
      }
      // go on after repair
    } else {
      tmp=newtmp;
    }

    // GCS against consumers
    for_set(Consumer,f->consumers,i)
      if(!back_constraint(&tmp,f,*i)) return; // type problem handled within
    // GCS against selectors
    if(f->selectors.size()) {
      Operator *fop = f->producer->op;
      ProtoType *fout = fop->signature->output;
      if (fout->isA("ProtoField")) {
    	  V4 << "FieldOps are ok now" << endl;
    	  // check that its a field of scalars?
      } else {
    	  ProtoType *newtmp = ProtoType::gcs(tmp,new ProtoScalar());
          if (newtmp) {
    	     tmp = newtmp;
          } else {
    	     type_err(f,"non-scalar selector "+f->to_str()); return;
          }
      }
    }
    maybe_set_range(f,tmp);
    
    /*
    if(f->range->type_of()=="ProtoTuple") { // look for tuple->vector upgrades
      ProtoTuple* tt = &dynamic_cast<ProtoTuple &>(*f->range);
      for(int i=0;i<tt->types.size();i++) 
        if(!tt->types[i]->isA("ProtoScalar")) return; // all scalars?
      ProtoVector* v = new ProtoVector(tt->bounded); v->types = tt->types;
      maybe_set_range(f,v);
    }
    */
  }

  // At each round, consider types of all neighbors... 
  // GCS against each that resolves
  // Resolve shouldn't care about the current type!

  void act(OperatorInstance* oi) {
    V3 << "Considering op instance "<<ce2s(oi)<<endl;
     //check if number of inputs is legal
    if(!oi->op->signature->legal_length(oi->inputs.size())) {
      compile_error(oi,oi->to_str()+" has "+i2s(oi->inputs.size())+" argumen"+
                    "ts, but signature is "+oi->op->signature->to_str());
      return;
    }
    
    if(oi->op->isA("Primitive")) { // constrain against signature
      // new-style resolution
      if(oi->op->marked(":type-constraints")) {
        TypeConstraintApplicator tca(this);
        if(tca.apply_constraints(oi,get_sexp(oi->op,":type-constraints")))
          note_change(oi);
      }
      
      V4 << "Attributes of " << ce2s(oi) << ": " << endl;
      map<string,Attribute*>::const_iterator end = oi->attributes.end();
      for( map<string,Attribute*>::const_iterator it = oi->attributes.begin(); it != end; ++it) {
         V4 << "- " << it->first << endl;
      }
      if(oi->attributes.count("LETFED-MUX")) {
        V4 << "LETFED-MUX in oi: " << ce2s(oi) << endl;
        V4 << "output is: " << ce2s(oi->output->range) 
           << ((oi->output->range->isA("DerivedType"))?"(Derived)":"(non-derived)") 
           << ((oi->output->range->isLiteral())?"(Literal)":"(non-literal)") 
           << endl;
        V4 << "init is: " << ce2s(oi->inputs[1]->range) 
           << ((oi->inputs[1]->range->isA("DerivedType"))?"(Derived)":"(non-derived)") 
           << ((oi->inputs[1]->range->isLiteral())?"(Literal)":"(non-literal)") 
           << endl;
      }
      if(oi->attributes.count("LETFED-MUX")
//                && oi->output->range->isA("DerivedType")
         ){
        // letfed mux resolves from init
        if(oi->inputs.size()<2)
          compile_error(oi,"Can't resolve letfed type: not enough mux arguments");
        Field* init = oi->inputs[1]; // true input = init
        if(!init->range->isA("DerivedType")) {
          V4 << "Resolving LETFED-MUX from init: " << ce2s(init->range) << endl;
          maybe_set_range(oi->output,Deliteralization::deliteralize(init->range));
        }
      }
      // ALSO: find GCS of producer, consumers, & field values
    } else if(oi->op->isA("Parameter")) { // constrain vs all calls, signature
      // find LCS of input types
      Parameter* p = &dynamic_cast<Parameter &>(*oi->op);
      OIset *srcs = &root->funcalls[p->container];
      ProtoType* inputs = NULL;
      for_set(OI*,*srcs,i) {
        if((*i)->op->isA("Literal")) return; // can't work with lambdas
        ProtoType* ti = (*i)->nth_input(p->index);
        inputs = inputs? ProtoType::lcs(inputs,ti) : ti;
      }
      ProtoType* sig_type = p->container->signature->nth_type(p->index);
      inputs = inputs ? ProtoType::gcs(inputs,sig_type) : sig_type;
      if(!inputs) return;
      // then take GCS of that against current field value
      ProtoType* newtype = ProtoType::gcs(oi->output->range,inputs);
      if(!newtype) {compile_error(oi,"type conflict for "+oi->to_str());return;}
      maybe_set_range(oi->output,newtype);
    } else if(oi->op->isA("CompoundOp")) { // constrain against params & output
      ProtoType* rtype = dynamic_cast<CompoundOp &>(*oi->op).output->range;
      ProtoType* newtype = ProtoType::gcs(rtype,oi->output->range);
      maybe_set_range(oi->output,newtype);
    } else if(oi->op->isA("Literal")) { // ignore: already be fully resolved
      // ignored
    } else {
      ierror("Don't know how to do type inference on undefined operators");
    }
  }

  // AMs that are the body of a CompoundOp may affect their signature
  void act(AmorphousMedium* am) {
    CompoundOp* f = am->bodyOf; if(f==NULL) return; // only CompoundOp ams
    if(!ProtoType::equal(f->signature->output,f->output->range)) {
      V2<<"Changing signature output of "<<ce2s(f)<<" to "<<ce2s(f->output->range)<<endl;
      f->signature->output = f->output->range;
      note_change(am);
    }
  }
};

/*****************************************************************************
 *  CONSTANT FOLDING                                                         *
 *****************************************************************************/

// NOTE: constantfolder is *not* a type checker!

// scalars & shorter vectors are treated as having 0s all the way out
int compare_numbers(ProtoType* a, ProtoType* b) { // 1-> a>b; -1-> a<b; 0-> a=b
  if(a->isA("ProtoScalar") && b->isA("ProtoScalar")) {
    return (S_VAL(a)==S_VAL(b)) ? 0 : ((S_VAL(a)>S_VAL(b)) ? 1 : -1);
  } else if(a->isA("ProtoScalar") || b->isA("ProtoScalar")) {
    int sx = a->isA("ProtoScalar") ? -1 : 1;
    double s = S_VAL(a->isA("ProtoScalar")?a:b);
    ProtoTuple* v = T_TYPE(a->isA("ProtoScalar")?b:a);
    if(s==S_VAL(v->types[0])) {
      for(int i=1;i<v->types.size();i++) {
        if(S_VAL(v->types[i])!=0) return sx*((S_VAL(v->types[1])>0)?1:-1);
      }
      return 0;
    } else return (sx * ((S_VAL(v->types[0]) > s) ? 1 : -1));
  } else { // 2 vectors
    ProtoTuple *va = T_TYPE(a), *vb = T_TYPE(b);
    int la = va->types.size(), lb = vb->types.size(), len = max(la, lb);
    for(int i=0;i<len;i++) {
      double sa=(i<la)?S_VAL(va->types[i]):0, sb=(i<lb)?S_VAL(vb->types[i]):0;
      if(sa!=sb) { return (sa>sb) ?  1 : -1; }
    }
    return 0;
  }
}

ProtoNumber* add_consts(ProtoType* a, ProtoType* b) {
  if(a->isA("ProtoScalar") && b->isA("ProtoScalar")) {
    return new ProtoScalar(S_VAL(a)+S_VAL(b));
  } else if(a->isA("ProtoScalar") || b->isA("ProtoScalar")) {
    double s = S_VAL(a->isA("ProtoScalar")?a:b);
    ProtoTuple* v = T_TYPE(a->isA("ProtoScalar")?b:a);
    ProtoVector* out = new ProtoVector(v->bounded);
    for(int i=0;i<v->types.size();i++) { 
       ProtoScalar* element = &dynamic_cast<ProtoScalar &>(*v->types[i]);
       out->add(new ProtoScalar(element->value));
    }
    dynamic_cast<ProtoScalar &>(*out->types[0]).value += s;
    return out;
  } else { // 2 vectors
    ProtoTuple *va = T_TYPE(a), *vb = T_TYPE(b);
    ProtoVector* out = new ProtoVector(va->bounded && vb->bounded);
    int la = va->types.size(), lb = vb->types.size(), len = max(la, lb);
    for(int i=0;i<len;i++) {
      double sa=(i<la)?S_VAL(va->types[i]):0, sb=(i<lb)?S_VAL(vb->types[i]):0;
      out->add(new ProtoScalar(sa+sb));
    }
    return out;
  }
}

class ConstantFolder : public IRPropagator {
 public:
  ConstantFolder(DFGTransformer* parent, Args* args) : IRPropagator(false,true) {
    verbosity = args->extract_switch("--constant-folder-verbosity") ? 
      args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream* out=0) { *out << "ConstantFolder"; }

  // FieldOp compatible accessors
  ProtoType* nth_type(OperatorInstance* oi, int i) {
    ProtoType* src = oi->inputs[i]->range;
    if(oi->op->isA("FieldOp")) src = F_VAL(src);
    return src;
  }
  double nth_scalar(OI* oi, int i) {return S_VAL(nth_type(oi,i));}
  ProtoTuple* nth_tuple(OI* oi, int i) { return T_TYPE(nth_type(oi,i)); }
  void maybe_set_output(OperatorInstance* oi,ProtoType* content) {
    if(oi->op->isA("FieldOp")) content = new ProtoField(content);
    maybe_set_range(oi->output,content);
  }

  void act(OperatorInstance* oi) {
    if(!oi->op->isA("Primitive")) return; // only operates on primitives
    //if(oi->output->range->isLiteral()) return; // might change...
    const string &name
      = ((oi->op->isA("FieldOp"))
         ? dynamic_cast<FieldOp &>(*oi->op).base->name
         : dynamic_cast<Primitive &>(*oi->op).name);
    // handled by type inference: elt, min, max, tup, all argk pass-throughs
    if(name=="mux") { 
      if(oi->inputs[0]->range->isLiteral()) { // case 1: arg0 is literal
        maybe_set_output(oi,oi->inputs[nth_scalar(oi,0) ? 1 : 2]->range);
      } else if(oi->inputs[1]->range->isLiteral() && // case 2: branches equal
                ProtoType::equal(oi->inputs[1]->range,oi->inputs[2]->range)) {
        maybe_set_output(oi,oi->inputs[1]->range);
      }
      return;
    } else if(name=="len") { // len needs only tuple to be bounded
      ProtoTuple* tt = nth_tuple(oi,0);
      if(tt->bounded) 
        maybe_set_output(oi,new ProtoScalar(tt->types.size()));
      return;
    }

    // rest only apply if all inputs are literals
    for(int i=0;i<oi->inputs.size();i++) {
      if(!oi->inputs[i]->range->isLiteral()) return;
    }
    if(name=="not") {
      maybe_set_output(oi,new ProtoBoolean(!nth_scalar(oi,0)));
    } else if(name=="+") {
      ProtoNumber* sum = new ProtoScalar(0);
      for(int i=0;i<oi->inputs.size();i++) sum=add_consts(sum,nth_type(oi,i));
      maybe_set_output(oi,sum);
    } else if(name=="-") {
      ProtoNumber* sum = new ProtoScalar(0); // sum all but first
      for(int i=1;i<oi->inputs.size();i++) 
        sum=add_consts(sum,nth_type(oi,i));
      // multiply by negative 1
      if(sum->isA("ProtoScalar")) { S_VAL(sum) *= -1;
      } else { // vector
        ProtoVector* s = &dynamic_cast<ProtoVector &>(*sum);
        for(int i=0;i<s->types.size();i++) S_VAL(s->types[i]) *= -1;
      }
      // add in first
      sum = add_consts(sum,nth_type(oi,0));
      maybe_set_output(oi,sum);
    } else if(name=="*") {
      double mults = 1;
      ProtoTuple* vnum = NULL;
      for(int i=0;i<oi->inputs.size();i++) {
        if(nth_type(oi,i)->isA("ProtoScalar")) { mults *= nth_scalar(oi,i);
        } else if(vnum) { compile_error(oi,">1 vector in multiplication");
        } else { vnum = nth_tuple(oi,i); 
        }
      }
      // final optional stage of vector multiplication
      if(vnum) {
        ProtoVector *pv = new ProtoVector(vnum->bounded);
        for(int i=0;i<vnum->types.size();i++)
          pv->add(new ProtoScalar(S_VAL(vnum->types[i])*mults));
        maybe_set_output(oi,pv);
      } else {
        maybe_set_output(oi,new ProtoScalar(mults));
      }
    } else if(name=="/") {
      double denom = 1;
      for(int i=1;i<oi->inputs.size();i++) denom *= nth_scalar(oi,i);
      if(oi->inputs[0]->range->isA("ProtoScalar")) {
        double num = nth_scalar(oi,0);
        maybe_set_output(oi,new ProtoScalar(num/denom));
      } else if(oi->inputs[0]->range->isA("ProtoVector")) { // vector
        ProtoTuple *num = nth_tuple(oi,0);
        ProtoVector *pv = new ProtoVector(num->bounded);
        for(int i=0;i<num->types.size();i++)
          pv->add(new ProtoScalar(S_VAL(num->types[i])/denom));
        maybe_set_output(oi,pv);
      }
    } else if(name==">") {
      int cp = compare_numbers(nth_type(oi,0),nth_type(oi,1));
      maybe_set_output(oi,new ProtoBoolean(cp==1));
    } else if(name=="<") {
      int cp = compare_numbers(nth_type(oi,0),nth_type(oi,1));
      maybe_set_output(oi,new ProtoBoolean(cp==-1));
    } else if(name=="=") {
      int cp = compare_numbers(nth_type(oi,0),nth_type(oi,1));
      maybe_set_output(oi,new ProtoBoolean(cp==0));
    } else if(name=="<=") {
      int cp = compare_numbers(nth_type(oi,0),nth_type(oi,1));
      maybe_set_output(oi,new ProtoBoolean(cp!=1));
    } else if(name==">=") {
      int cp = compare_numbers(nth_type(oi,0),nth_type(oi,1));
      maybe_set_output(oi,new ProtoBoolean(cp!=-1));
    } else if(name=="abs") {
      maybe_set_output(oi,new ProtoScalar(fabs(nth_scalar(oi,0))));
    } else if(name=="floor") {
      maybe_set_output(oi,new ProtoScalar(floor(nth_scalar(oi,0))));
    } else if(name=="ceil") {
      maybe_set_output(oi,new ProtoScalar(ceil(nth_scalar(oi,0))));
    } else if(name=="round") {
      maybe_set_output(oi,new ProtoScalar(rint(nth_scalar(oi,0))));
    } else if(name=="mod") {
      //TODO: this is also implemented in kernel/proto.c,
      //      merge these implementations!
      double dividend = nth_scalar(oi,0), divisor = nth_scalar(oi,1);
      double val = fmod(fabs(dividend), fabs(divisor)); 
      if(divisor < 0 && dividend < 0) val *= -1;
      else if(dividend < 0) val = divisor - val;
      else if(divisor < 0) val += divisor;
      maybe_set_output(oi,new ProtoScalar(val));
    } else if(name=="rem") {
      double a = nth_scalar(oi,0), b = nth_scalar(oi,1);
      maybe_set_output(oi,new ProtoScalar(fmod(a,b)));
    } else if(name=="pow") {
      double a = nth_scalar(oi,0), b = nth_scalar(oi,1);
      maybe_set_output(oi,new ProtoScalar(pow(a,b)));
    } else if(name=="sqrt") { 
      maybe_set_output(oi,new ProtoScalar(sqrt(nth_scalar(oi,0))));
    } else if(name=="log") {
      maybe_set_output(oi,new ProtoScalar(log(nth_scalar(oi,0))));
    } else if(name=="sin") {
      maybe_set_output(oi,new ProtoScalar(sin(nth_scalar(oi,0))));
    } else if(name=="cos") {
      maybe_set_output(oi,new ProtoScalar(cos(nth_scalar(oi,0))));
    } else if(name=="tan") {
      maybe_set_output(oi,new ProtoScalar(tan(nth_scalar(oi,0))));
    } else if(name=="asin") {
      maybe_set_output(oi,new ProtoScalar(asin(nth_scalar(oi,0))));
    } else if(name=="acos") {
      maybe_set_output(oi,new ProtoScalar(acos(nth_scalar(oi,0))));
    } else if(name=="atan2") {
      double a = nth_scalar(oi,0), b = nth_scalar(oi,1);
      maybe_set_output(oi,new ProtoScalar(atan2(a,b)));
    } else if(name=="sinh") {
      maybe_set_output(oi,new ProtoScalar(sinh(nth_scalar(oi,0))));
    } else if(name=="cosh") {
      maybe_set_output(oi,new ProtoScalar(cosh(nth_scalar(oi,0))));
    } else if(name=="tanh") {
      maybe_set_output(oi,new ProtoScalar(tanh(nth_scalar(oi,0))));
    } else if(name=="vdot") {
      // check two, equal length vectors
      ProtoVector* v1 = NULL;
      ProtoVector* v2 = NULL;
      if(2==oi->inputs.size()
         && nth_type(oi,0)->isA("ProtoVector")
         && nth_type(oi,1)->isA("ProtoVector")) {
        v1 = &dynamic_cast<ProtoVector &>(*nth_type(oi,0));
        v2 = &dynamic_cast<ProtoVector &>(*nth_type(oi,1));
        if(v1->types.size() != v2->types.size()) {
          compile_error("Dot product requires 2, *equal size* vectors");
        }
      } else {
        compile_error("Dot product requires exactly *2* vectors");
      }
      // sum of products
      ProtoScalar* sum = new ProtoScalar(0);
      for(int i=0; i<v1->types.size(); i++) {
        sum->value += dynamic_cast<ProtoScalar &>(*v1->types[i]).value *
          dynamic_cast<ProtoScalar &>(*v2->types[i]).value;
      }
      maybe_set_output(oi,sum);
    } else if(name=="min-hood" || name=="max-hood" || name=="any-hood"
              || name=="all-hood") {
      maybe_set_output(oi,F_VAL(oi->inputs[0]->range));
    }
  }
  
};

/*****************************************************************************
 *  LITERALIZER                                                              *
 *****************************************************************************/

class Literalizer : public IRPropagator {
 public:
  Literalizer(DFGTransformer* parent, Args* args) : IRPropagator(true,true) {
    verbosity = args->extract_switch("--literalizer-verbosity") ? 
      args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream* out=0) { *out << "Literalizer"; }
  void act(Field* f) {
    if(f->producer->op->isA("Literal")) return; // literals are already set
    if(f->producer->op->attributes.count(":side-effect")) return; // keep sides
    if(f->range->isLiteral()) {
      OI *oldoi = f->producer;
      OI *newoi = root->add_literal(f->range,f->domain,oldoi)->producer;
      V2<<"Literalizing:\n  "<<ce2s(oldoi)<<" \n  into "<<ce2s(newoi)<<endl;
      root->relocate_consumers(f,newoi->output); note_change(f);
      root->delete_node(oldoi); // kill old op
    }
  }

  void act(OperatorInstance* oi) {
    if(!oi->op->isA("Primitive")) return; // only operates on primitives
    string name = dynamic_cast<Primitive &>(*oi->op).name;
    // change "apply" of literal lambda into just an OI of that operator
    if(name=="apply") {
      if(oi->inputs[0]->range->isA("ProtoLambda") && 
         oi->inputs[0]->range->isLiteral()) {
        OI* newoi=new OI(oi,L_VAL(oi->inputs[0]->range),oi->output->domain);
        for(int i=1;i<oi->inputs.size();i++) 
          newoi->add_input(oi->inputs[i]);
        V2<<"Literalizing:\n  "<<ce2s(oi)<<" \n  into "<<ce2s(newoi)<<endl;
        root->relocate_consumers(oi->output,newoi->output); note_change(oi);
        root->delete_node(oi); // kill old op
      }
    }
  }
};

/*****************************************************************************
 *  DEAD CODE ELIMINATOR                                                     *
 *****************************************************************************/

//  - mark the output and each Operator w. a side-effect attribute["SideEffect"]
//  - mark every node as "dead"
//  - erase marks on everything leading to a side-effect or an output
//  - erase everything still marked as "dead"

map<string,pair<bool,int> > arg_mem;
int mem_arg(Args* args,string name,int defaultv) {
  if(!arg_mem.count(name)) {
    if(args->extract_switch(name.c_str()))
      arg_mem[name] = make_pair<bool,int>(true,args->pop_int());
    else
      arg_mem[name] = make_pair<bool,int>(false,-1);
  }
  return arg_mem[name].first ? arg_mem[name].second : defaultv;
}

class DeadCodeEliminator : public IRPropagator {
 public:
  Fset kill_f; AMset kill_a;
  DeadCodeEliminator(DFGTransformer* par, Args* args)
    : IRPropagator(true,false,true) {
    verbosity=mem_arg(args,"--dead-code-eliminator-verbosity",par->verbosity);
  }
  
  virtual void print(ostream* out=0) { *out << "DeadCodeEliminator"; }
  void preprop() { kill_f = worklist_f; kill_a = worklist_a; }
  void postprop() {
    any_changes = !kill_f.empty() || !kill_a.empty();
    while(!kill_f.empty()) {
      Field* f = *kill_f.begin(); kill_f.erase(f);
      V2<<"Deleting field "<<ce2s(f)<<endl;
      root->delete_node(f->producer);
    }
    while(!kill_a.empty()) {
      AM* am = *kill_a.begin(); kill_a.erase(am);
      V2<<"Deleting AM "<<ce2s(am)<<endl;
      root->delete_space(am);
    }
  }
  
  void act(Field* f) {
	V5 << "Dead Code Eliminator examining field " << ce2s(f) << endl;
    if(!kill_f.count(f)) return; // only check the dead
    bool live=false; string reason = "";
    if(f->is_output()) { live=true; reason="output";} // output is live
    if(f->producer->op->attributes.count(":side-effect"))
      { live=true; reason="side effect"; }
    for_set(Consumer,f->consumers,i) { // live consumer -> live
      V5 << "Consumer: " << ce2s((*i).first) << endl;
      // ignore non-called functions
      if(root->relevant.count((*i).first->domain()->root())==0) continue;
      // check for live consumers
      if(!kill_f.count((*i).first->output)) 
        {live=true;reason="consumer";break;}
    }
    for_set(AM*,f->selectors,ai) // live selector -> live
      if(!kill_a.count(*ai)) {live=true;reason="selector";break;}
    V4<<"Is field "<<ce2s(f)<<" live? "<<b2s(live)<<"("<<reason<<")"<<endl;
    if(live) { kill_f.erase(f); note_change(f); }
  }
  
  void act(AM* am) {
	V5 << "Dead Code Eliminator examining AM " <<ce2s(am) << endl;
    if(!kill_a.count(am)) return; // only check the dead
    bool live=false;
    for_set(AM*,am->children,i)
      if(!kill_a.count(*i)) {live=true;break;} // live child -> live
    for_set(Field*,am->fields,i)
      if(!kill_f.count(*i)) {live=true;break;} // live domain -> live
    if(am->bodyOf!=NULL) {live=true;} // don't delete root AMs
    if(live) { kill_a.erase(am); note_change(am); }
  }
};


/*****************************************************************************
 *  INLINING                                                                 *
 *****************************************************************************/

#define DEFAULT_INLINING_THRESHOLD 10
class FunctionInlining : public IRPropagator {
 public:
  int threshold; // # ops to make something an inlining target
  FunctionInlining(DFGTransformer* parent, Args* args)
    : IRPropagator(false,true,false) {
    verbosity = args->extract_switch("--function-inlining-verbosity") ? 
      args->pop_int() : parent->verbosity;
    threshold = args->extract_switch("--function-inlining-threshold") ?
      args->pop_int() : DEFAULT_INLINING_THRESHOLD;
  }
  virtual void print(ostream* out=0) { *out << "FunctionInlining"; }
  
  void act(OperatorInstance* oi) {
    if(!oi->op->isA("CompoundOp")) return; // can only inline compound ops
    if(oi->recursive()) return; // don't inline recursive
    // check that either the body or the container is small
    Fset bodyfields;
    int bodysize = dynamic_cast<CompoundOp &>(*oi->op).body->size();
    int containersize = oi->output->domain->root()->size();
    if(threshold!=-1 && bodysize>threshold && containersize>threshold) return;
    
    // actually carry out the inlining
    V2<<"Inlining function "<<ce2s(oi)<<endl;
    note_change(oi); root->make_op_inline(oi);
  }
};


/*****************************************************************************
 *  TOP-LEVEL ANALYZER                                                       *
 *****************************************************************************/

// some test classes
class InfiniteLoopPropagator : public IRPropagator {
public:
  InfiniteLoopPropagator() : IRPropagator(true,false) {}
  void act(Field* f) { note_change(f); }
};
class NOPPropagator : public IRPropagator {
public:
  NOPPropagator() : IRPropagator(true,true,true) {}
};


ProtoAnalyzer::ProtoAnalyzer(NeoCompiler* parent, Args* args) {
  verbosity = args->extract_switch("--analyzer-verbosity") ? 
    args->pop_int() : parent->verbosity;
  max_loops=args->extract_switch("--analyzer-max-loops")?args->pop_int():10;
  paranoid = args->extract_switch("--analyzer-paranoid")|parent->paranoid;
  // set up rule collection
  rules.push_back(new TypePropagator(this,args));
  rules.push_back(new ConstantFolder(this,args));
  rules.push_back(new Literalizer(this,args));
  rules.push_back(new DeadCodeEliminator(this,args));
  rules.push_back(new FunctionInlining(this,args));
}


/*****************************************************************************
 *  GLOBAL-TO-LOCAL TRANSFORMER                                              *
 *****************************************************************************/

// From interpreter.
Operator *op_err(CompilationElement *where, string msg);

// Change neighborhood operations to fold-hoods.
//
// Mechanism:
// 1. Find the summary operation.
// 2. Get the tree of field ops leading to it and turn them into a compound op.
// 3. Combine inputs to nbr ops into a tuple input.
// 4. Mark inputs to locals with "loopref".  (XXX ?)
// 5. ???
// 6. Profit!

class HoodToFolder : public IRPropagator {
public:
  HoodToFolder(GlobalToLocal *parent, Args *args) : IRPropagator(false, true) {
    verbosity = args->extract_switch("--hood-to-folder-verbosity") ?
      args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream *out = 0) { *out << "HoodToFolder"; }
  void act(OperatorInstance *oi);

private:
  CEmap(CompoundOp *, CompoundOp *) localization_cache;
  Operator *localize_operator(Operator *op);
  CompoundOp *localize_compound_op(CompoundOp *cop);
  CompoundOp *tuplize_inputs(CompoundOp *cop);
  CompoundOp *add_detuplization(CompoundOp *cop);
  CompoundOp *make_nbr_subroutine(OperatorInstance *oi, Field **exportf);
  Operator *nbr_op_to_folder(OperatorInstance *oi);
  void restrict_elements(OIset* elts,vector<Field *>* exports, AM* am,OI* summary_op);
};

/**
 * Turn the inputs of this compound op into a tuple
 * This is done when we change the output to a tuple and need the inputs to match,
 *   since the function is used recursively
 */
CompoundOp *
HoodToFolder::tuplize_inputs(CompoundOp *cop)
{
	Signature* sig = cop->signature;
	V3 << "Signature before tuplize " << ce2s(sig) << endl;
	ProtoTuple* ret = new ProtoTuple(!sig->rest_input);
	for(int i=0; i<sig->n_fixed(); i++) {
	   ProtoType* elt = ProtoType::clone(sig->nth_type(i));
	   ret->add(elt);
	}
	 if(sig->rest_input!=NULL) // rest is unbounded end to tuple
	   ret->add(sig->rest_input);

    V3 << "Tuplize inputs tup ret " << ce2s(ret) << endl;

	sig->required_inputs.clear();
	sig->optional_inputs.clear();
	sig->required_inputs.push_back(ret);
	sig->rest_input = NULL;

	V3 << "Tuplize inputs new Signature " << ce2s(sig) << endl;
	return cop;
}

/**
 * Detuplization:
 *   Only called when the input is a tuple
 *    Create a new parameter for the tuple input
 *    Replace all Parameters with an elt
 *      link the input to the elt to the tuple parameter
 *      link anything that had the original parameter as an input to the output of the elt instead
 */
CompoundOp *
HoodToFolder::add_detuplization(CompoundOp *cop)
{
	V3 << "Sig in add_detup " << ce2s(cop->signature) << endl;

	Field* tupParam = NULL;

	CEmap(OI*,OI*) paramMap;
	OIset ois;
	cop->body->all_ois(&ois);
	int eltIndex = 0;

	for_set(OI *, ois, i) {
	  OI *oi = *i;
	  V4 << "Examining operator " << ce2s(oi->op) << endl;
	  if (oi->op->isA("Parameter")) {
		  if (tupParam == NULL) {
			tupParam = root->add_parameter(cop,make_gensym("tuparg")->name,eltIndex,cop->body,oi);
			tupParam->range = new ProtoTuple();
			V3 << "Adding new tup Parameter " << ce2s(tupParam) << endl;
		  }
		  OI* newElt = new OperatorInstance(cop, Env::core_op("elt"), cop->body);
		  paramMap[oi] = newElt;
		  newElt->output->range = oi->output->range;
		  ProtoScalar* si = new ProtoScalar(eltIndex);
		  Field* sField = root->add_literal(si, oi->domain(), oi);
		  eltIndex++;
		  newElt->add_input(tupParam);
		  newElt->add_input(sField);
		  V3 << "Adding new Elt " << ce2s(newElt) << endl;
	  }
	}

	for_set(OI *, ois, i) {
	   OI *oi = *i;
	 
	   for (int j=0; j<oi->inputs.size(); ++j) {
		  Field* iField = oi->inputs[j];
		  OI* mOi = paramMap[iField->producer];
		  if (mOi != NULL) {
			  V5 << "Relocating input " << j << " for " << ce2s(oi) << " to " << ce2s(mOi) << endl;
			  root->relocate_source(oi, j, mOi->output);
		  }
	   }
	}

}

void
HoodToFolder::act(OperatorInstance *oi)
{
  // Conversions begin at summaries (field-to-local ops).
  if (oi->inputs.size() == 1 && oi->inputs[0] != NULL
      && oi->inputs[0]->range->isA("ProtoField")
      && oi->output->range->isA("ProtoLocal")) {
    V2 << "Changing to fold-hood: " << ce2s(oi) << endl;
    // (fold-hood-plus folder fn input)
    AM *space = oi->output->domain;
    Field *exportf;
    CompoundOp *nbrop = make_nbr_subroutine(oi, &exportf);
    Operator *folder = nbr_op_to_folder(oi);
    OI *noi = new OperatorInstance(oi, Env::core_op("fold-hood-plus"), space);
    // Hook the inputs up to the fold-hood.
    V3 << "Connecting inputs to foldhood\n";
    noi->add_input(root->add_literal(new ProtoLambda(folder), space, oi));
    noi->add_input(root->add_literal(new ProtoLambda(nbrop), space, oi));
    noi->add_input(exportf);
    // Switch consumers and quit.
    V3 << "Changing over consumers\n";
    root->relocate_consumers(oi->output, noi->output); note_change(oi);
    root->delete_node(oi);
  }
}

Operator *
HoodToFolder::localize_operator(Operator *op)
{
  V4 << "Localizing operator: " << ce2s(op) << endl;
  if (op->isA("Literal")) {
    Literal *literal = &dynamic_cast<Literal &>(*op);
    // Strip any field.
    if (literal->value->isA("ProtoField"))
      return new Literal(op, F_VAL(literal->value));
    else
      return op;
  } else if (op->isA("Primitive")) {
    Operator *local = LocalFieldOp::get_local_op(op);
    return local ? local : op;
  } else if (op->isA("CompoundOp")) {
    return localize_compound_op(&dynamic_cast<CompoundOp &>(*op));
  } else if (op->isA("Parameter")) {
    // Parameters are always OK.
    return op;
  } else {
    ierror("Don't know how to localize operator: " + ce2s(op));
  }
}

CompoundOp *
HoodToFolder::localize_compound_op(CompoundOp *cop)
{
  V5 << "Checking localization cache\n";
  if (localization_cache.count(cop))
    return localization_cache[cop];

  // If it's already pointwise, just return.
  V5 << "Checking whether op is already pointwise\n";
  Fset fields;
  cop->body->all_fields(&fields);
  bool local = true;
  for_set(Field *, fields, i) {
    V5 << "Considering field " << ce2s(*i) << endl;
    if ((*i)->range->isA("ProtoField"))
      local = false;
  }
  if (local)
    return cop;

  // Walk through ops: local & nbr ops are flattened, others are localized.
  V5 << "Transforming op to local\n";
  CompoundOp *new_cop = new CompoundOp(cop);
  OIset ois;
   new_cop->body->all_ois(&ois);
   for_set(OI *, ois, i) {
     OI *oi = *i;
     if (oi->op == Env::core_op("nbr")) {
      root->relocate_consumers(oi->output, oi->inputs[0]);
    } else if (oi->op == Env::core_op("local")) {
      ierror("No locals should be found in an extracted hood function");
    } else if (oi->op == Env::core_op("restrict")) {
      // restrict is not change to reference here: it will be handled later.
      OI *src = (oi->inputs[0] ? oi->inputs[0]->producer : 0);
      if (src != 0 && src->op == Env::core_op("local")) {
        root->relocate_source(oi, 0, src->inputs[0]);
        V4 << "Relocated source: " << ce2s(oi) << endl;
        V4 << "OI->Inputs[0] = " << ce2s(oi->inputs[0]) << endl;
      }
    } else if (!oi->op->isA("FieldOp") && oi->op->isA("Primitive") && (oi->op->signature->output->isA("ProtoField"))) {
    	V2 << "Output is a Field but this isn't a FieldOp, this must be a primitive field function" << endl;
    	// We can localize it using localize_operator, but we want to keep the op name the same, since it will emit as the primitive
    	string pName = oi->op->name;
    	oi->op = localize_operator(oi->op);
    	// Fix output.
    	oi->output->range = oi->op->signature->output;
    	// Set the name back
    	oi->op->name = pName;
    } else {
      oi->op = localize_operator(oi->op);
      // Fix output.
      oi->output->range = oi->op->signature->output;
    }
  }
  localization_cache[cop] = new_cop;
  return new_cop;
}

/// If any elements in OIset* have domains in am's parents, 
/// replace them with copies restricted to am.  Likewise,
/// change exports to reference restricted versions
void
HoodToFolder::restrict_elements(OIset* elts,vector<Field *>* exports,AM* am,OI* summary_op) {
  CEmap(OI*,OI*) omap; // remapped OIs
  CEmap(Field*,Field*) fmap; // remapped fields

  // create all replacements
  for_set(OI*,*elts,oi) {
    if((*oi)->op==Env::core_op("restrict")) {
      omap[*oi] = NULL;  // Restrictions are simply elided
    } else if((*oi)->output->domain == am || 
              (*oi)->output->domain->child_of(am)) {
      continue; // ignore elements that don't need restriction
    } else if(am->child_of((*oi)->output->domain)) {
      // remap elements computed with a larger domain to a new instance
      omap[*oi] = new OperatorInstance(*oi,(*oi)->op,am);
      omap[*oi]->output->range = (*oi)->output->range;
      fmap[(*oi)->output] = omap[*oi]->output;
      for(int i=0;i<(*oi)->inputs.size();i++)
        omap[*oi]->add_input((*oi)->inputs[i]);
    } else {
      ierror("Can't restrict "+(*oi)->output->to_str()+" to "+am->to_str());
    }
  }

  // remap restriction outputs
  for_set(OI*,*elts,oi) {
    if((*oi)->op==Env::core_op("restrict")) {
      // search upstream for a non-restrict input
      OI* src = *oi; 
      while(src->op==Env::core_op("restrict")) { src=src->inputs[0]->producer; }
      // ensure this is part of the restricted set (it should always be)
      if(!elts->count(src) || !omap.count(src)) 
        ierror("Restrict leads out of expected neighborhood computation ops");
      fmap[(*oi)->output] = fmap[src->output];
    }
  }

  // change contents of elts
  for_map(OI*,OI*,omap,i) {
    elts->erase(i->first);
    if(i->second!=NULL) { elts->insert(i->second); }
  }
  // remap all input fields (outputs have been handled by OI copying)
  for_set(OI*,*elts,oi) {
    for(int i=0;i<(*oi)->inputs.size();i++)
      if(fmap.count((*oi)->inputs[i]))
        root->relocate_source(*oi,i,fmap[(*oi)->inputs[i]]);
  }
  // swap output if appropriate
  if(fmap.count(summary_op->inputs[0]))
    root->relocate_source(summary_op,0,fmap[summary_op->inputs[0]]);

  // Now walk through the exports, restricting and replacing as needed
  for(int i=0;i<exports->size();i++) {
    Field* ef = (*exports)[i];
    if(ef->domain == am || ef->domain->child_of(am)) {
      continue; // ignore elements that don't need restriction
    } else if(am->child_of(ef->domain)) {
      // create a new restriction
      OI* restrict = new OperatorInstance(ef,Env::core_op("restrict"),am);
      restrict->add_input(ef); 
      restrict->add_input(am->selector); // selector known to be non-null
      restrict->output->range = ef->range; // copy range
      // remap elements that used the old export to the new restricted one
      for_set(OI*,*elts,oi) {
        for(int j=0;j<(*oi)->inputs.size();j++) {
          if((*oi)->inputs[j] == ef) {
            root->relocate_source(*oi,j,restrict->output);
          }
        }
      }
      // change the export
      (*exports)[i] = restrict->output;
    } else {
      ierror("Can't restrict export "+ef->to_str()+" to "+am->to_str());
    }    
  }
}

CompoundOp *
HoodToFolder::make_nbr_subroutine(OperatorInstance *oi, Field **exportf)
{
  V3 << "Creating subroutine for: " << ce2s(oi) << endl;

  // First, find all field-valued ops feeding this summary operator.
  OIset elts;
  vector<Field *> exports;
  OIset q;
  q.insert(oi);
  while (q.size()) {
    OperatorInstance *next = *q.begin();
    q.erase(next);
    for (size_t i = 0; i < next->inputs.size(); i++) {
      Field *f = next->inputs[i];
      // Record exported fields.
      if (f->producer->op == Env::core_op("nbr")) {
        if (!f->producer->inputs.size())
          ierror("No input for nbr operator: " + f->producer->to_str());
        if (index_of(&exports, f->producer->inputs[0]) == -1)
          exports.push_back(f->producer->inputs[0]);
      }
      if (f->range->isA("ProtoField")
          && !elts.count(f->producer)
          && f->producer->op != Env::core_op("local"))
        { elts.insert(f->producer); q.insert(f->producer); }
    }
  }

  V3 << "Found " << elts.size() << " elements" << endl;
  for_set(OI*,elts,e) { V5 << (*e)->to_str() << endl; }
  V3 << "Found " << exports.size() << " exports" << endl;
  for(int i=0;i<exports.size();i++) { V5 << exports[i]->to_str() << endl; }

  // Some of the computation elements may have been computed on a
  // larger AM, and need to be restricted.  This cannot happen in the
  // compound op form.  However, since there are no "if" ops allowed
  // within a neighborhood computation, it is safe to map all elements
  // of the neighborhood computation into restricted copies; computation
  // that would have happened on other neighbors would just be discarded
  // In practice, what happens here is that any "restrict" is bubbled up
  // to be be at the exports instead.
  restrict_elements(&elts,&exports,oi->output->domain,oi);
  V3 << "Restricted elements" << endl;
  for_set(OI*,elts,e) { V5 << (*e)->to_str() << endl; }
  V3 << "Restricted exports" << endl;
  for(int i=0;i<exports.size();i++) { V5 << exports[i]->to_str() << endl; }
  V5 << "Summary is now: " << oi->to_str() << endl;

  // Create the compound op.
  V4 << "Creating compound operator from elements" << endl;
  CompoundOp *cop
    = root->derive_op(&elts, oi->domain(), &exports, oi->inputs[0],"Hood");
  V5 << "Localizing new compound operator " << ce2s(cop) << endl;
  cop = localize_compound_op(cop);

  // Construct input structure.
  if (exports.size() == 0) {
    V4 << "Zero exports: adding a scratch export" << endl;
    // Add a scratch input.
    ProtoType *scratch = new ProtoScalar(0);
    *exportf = root->add_literal(scratch, oi->domain(), oi);
    cop->signature->required_inputs.push_back(scratch);
    cop->signature->names["arg0"] = 0;
  } else if (exports.size() == 1) {
    V4 << "One export: using "<< ce2s(exports[0]) << " directly" << endl;
    *exportf = exports[0];
  } else {
    V4 << "Multiple exports: binding into a tuple" << endl;
    OI *tup = new OperatorInstance(oi, Env::core_op("tup"), oi->domain());
    for (size_t i = 0; i < exports.size(); i++)
      tup->add_input(exports[i]);
    *exportf = tup->output;
    tuplize_inputs(cop);
    add_detuplization(cop);
    V5 << "Cop signature after tuplize " << ce2s(cop->signature) << endl;
    V5 << "Cop export after tuplize " << ce2s(tup->output) << endl;
  }

  vector<Field *> in;
  in.push_back(*exportf);

  // Add multiplier if needed.
  if (oi->op->name == "int-hood") {
    V3 << "Adding int-hood multiplier\n";
    Operator *infinitesimal = Env::core_op("infinitesimal");
    OI *oin = new OperatorInstance(oi, infinitesimal, cop->body);
    OI *tin = new OperatorInstance(oi, Env::core_op("*"), cop->body);
    tin->add_input(oin->output);
    tin->add_input(cop->output);
    cop->output = tin->output;
  }
  return cop;
}

Operator *
HoodToFolder::nbr_op_to_folder(OperatorInstance *oi)
{
  const string &name = oi->op->name;
  V3 << "Selecting folder for: " << name << endl;
  if      (name == "min-hood") return Env::core_op("min");
  else if (name == "max-hood") return Env::core_op("max");
  else if (name == "any-hood") return Env::core_op("max");
  else if (name == "all-hood") return Env::core_op("min");
  else if (name == "int-hood") return Env::core_op("+");
  else if (name == "sum-hood") return Env::core_op("+");
  else
    return
      op_err(oi, "Can't convert summary '" + name + "' to local operator");
}

// Changes restrict/mux complexes to branches
// Mechanism:
// 1. find complementary AM selector pairs
// 2. turn each sub-AM into a no-argument function
// 3. hook these functions into a branch operator and delete the old

class RestrictToBranch : public IRPropagator {
public:
  RestrictToBranch(GlobalToLocal* parent, Args* args) : IRPropagator(false,true) {
    verbosity = args->extract_switch("--restrict-to-branch-verbosity") ?
      args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream* out=0) { *out << "RestrictToBranch"; }

  CompoundOp* am_to_lambda(AM* space,Field *out,string stem) {
    // discard immediate-child restrict functions:
    V4 << "Converting 2-input 'restrict' operators to references\n";
    Fset fields; fields = space->fields; // delete invalidates original iterator
    for_set(Field*,fields,i) {
      V4 << "Checking field: " << ce2s(*i) << endl;
      V4 << "  Producer: " << ce2s((*i)->producer) << endl;
      if((*i)->producer->op==Env::core_op("restrict") &&
         (*i)->producer->inputs.size()==2) {
        V4 << "Converting to reference: "+ce2s((*i)->producer)+"\n";
        (*i)->producer->remove_input(1);
        (*i)->producer->op = Env::core_op("reference");
      }
    }
    // make fn from all operators in the space, and all its children
    V4 << "Deriving operators from space "+ce2s(space)+"\n";
    OIset elts; space->all_ois(&elts);
    vector<Field*> ins; // no inputs
    CompoundOp* res = root->derive_op(&elts,space,&ins,out,stem);
    for_set(OI *, elts, i) {
      V2 << "OI ELT: " << ce2s(*i) << endl;
    }
    return res;
  }
  void act(OperatorInstance* oi) {
	// properly formed muxes are candidates for if patterns
    if(oi->op==Env::core_op("mux") && oi->inputs.size()==3) {
      //&& !oi->attributes.count("LETFED-MUX")) {
      bool letfedmux = false;
      if (oi->attributes.count("LETFED-MUX")) {
    	  letfedmux = true;
      }
      V3 << "Considering If->Branch candidate:\n   "<< ce2s(oi) <<endl;
      OI *join; Field *test, *testnot; AM *trueAM, *falseAM; AM* space;
      // fill in blanks
      join = oi; space = oi->output->domain; test = oi->inputs[0];
      trueAM = oi->inputs[1]->domain;
      falseAM = oi->inputs[2]->domain;
      testnot = falseAM->selector;
      // check for validity
      V4 << "Checking not operator\n";
      if(!testnot) return;
      if(!(testnot->producer->op==Env::core_op("not")
           && testnot->selectors.size()==1
           && testnot->producer->inputs[0]==test)) return;
      for_set(Consumer,testnot->consumers,i) // only restricts can consume
        if(!(i->first->op==Env::core_op("restrict") && i->second==1)) return;
      V4 << "Checking true-expression space\n";
      if(!(trueAM->selector==test && trueAM->parent==space)) return;
      V4 << "Checking false-expression space\n";
      if(!(falseAM->selector==testnot && falseAM->parent==space)) return;
      OI* branch = new OperatorInstance(oi,Env::core_op("branch"),space);
      V3 << "Transforming to true AM to a lambda function" << endl;
      CompoundOp *tf = am_to_lambda(trueAM,oi->inputs[1],letfedmux?"Init":"TrueBranch");
      V3 << "Transforming to false AM to a lambda function" << endl;
      CompoundOp *ff = am_to_lambda(falseAM,oi->inputs[2],letfedmux?"Update":"FalseBranch");
      OI* newoi;
      if (!letfedmux) {
        // Swap the mux for a branch:
        V3 << "Transforming to branch\n";
        tf->body->mark("branch-fn"); ff->body->mark("branch-fn");
        branch->add_input(test);
        branch->add_input(root->add_literal(new ProtoLambda(tf),space,tf));
        branch->add_input(root->add_literal(new ProtoLambda(ff),space,ff));
        root->relocate_consumers(oi->output,branch->output);
        newoi = branch;
      } else {
         V3 << "ff Signature: " << ce2s(ff->signature) << endl;
         ff->signature->required_inputs.push_back(oi->inputs[2]->range);
         V3 << "ff Signature: " << ce2s(ff->signature) << endl;
    	  // Easier to create a new mux than move everything
    	 OI* newmux = new OperatorInstance(oi, Env::core_op("mux"), space);
       	 newmux->add_input(test);
    	 newmux->add_input(root->add_literal(new ProtoLambda(tf),space,tf));
    	 newmux->add_input(root->add_literal(new ProtoLambda(ff),space,ff));
    	 newmux->output->range = oi->output->range;
    	 root->relocate_consumers(oi->output, newmux->output);
    	 V5 << "Test: " << ce2s(test) << endl;
    	 for_set(Consumer,test->consumers,i) {
           V5 << "Test Consumer: " << ce2s((*i).first) << endl;
    	 }
    	 newoi = newmux;
      }
      V3 << "Removing old OIs" << endl;
      // delete old elements

      // 1) Find the new Delay, if there is one
      OI* newDelay = NULL;
      OIset br_ops1; tf->body->all_ois(&br_ops1); ff->body->all_ois(&br_ops1);
      for_set(OI*,br_ops1,i) {
      	  if ((*i)->op == Env::core_op("delay")) {
      		  if (newDelay == NULL) {
    		    newDelay = *i;
    	      } else {
    	    	  ierror("Multiple delays in the same update function, this is unexpected.");
    	      }
      	  }
      }

      V3 << "New Delay is " << ce2s(newDelay) << endl;
      OIset elts; trueAM->all_ois(&elts); falseAM->all_ois(&elts);
      // Move any other references to this delay to the new branch
      for_set(OI*,elts,i) {
    	V3 << "Removing OI: " << ce2s(*i) << endl;
    	if ((*i)->op == Env::core_op("delay")) {
          OI* delay = *i;
    	  std::vector<OI*> relocaters;
          V3 << "Delay output " << ce2s(delay->output) << endl;
    	  for_set(Consumer,delay->output->consumers,j) {
    	    OI* delayConsumer = (*j).first;
       		if (delayConsumer != NULL) {
    		  if (delayConsumer->op != NULL) {
    		    V3 << "Delay Consumer: " << ce2s(delayConsumer) << endl;
    			relocaters.push_back(delayConsumer);
    		  }
       		}
    	  }
    	  for (int k=0; k<relocaters.size(); ++k) {
    		  OI* delayConsumer = relocaters[k];
    		  V3 << "Relocating source from " << ce2s(delayConsumer) << " to " << ce2s(newDelay->output) << endl;
    		  root->relocate_source(delayConsumer, 0, newDelay->output);
    	  }
    	}
    	root->delete_node(*i);
      }
      root->delete_node(oi);
      root->delete_space(trueAM); root->delete_space(falseAM);
      root->delete_node(testnot->producer);
      // note all changes
      note_change(newoi);
      OIset br_ops; tf->body->all_ois(&br_ops); ff->body->all_ois(&br_ops);
      for_set(OI*,br_ops,i) {
    	  V5 << "Note change for " << ce2s(*i) << endl;
    	  note_change(*i);
      }
      V5 << "Test: " << ce2s(test) << endl;
      for_set(Consumer,test->consumers,i) {
         V5 << "Test Consumer: " << ce2s((*i).first) << endl;
      }
    }
  }
};

class RestrictToReference : public IRPropagator {
public:
  RestrictToReference(GlobalToLocal* parent, Args* args) : IRPropagator(false,true) {
    verbosity = args->extract_switch("--restrict-to-reference-verbosity") ?
      args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream* out=0) { *out << "RestrictToReference"; }

  void act(OperatorInstance* oi) {
      if(oi->op==Env::core_op("restrict") && oi->inputs.size()==1) {
    	  if (oi->inputs[0] != NULL) {
    		  V3 << "Converting restrict to reference: " << ce2s(oi) << endl;
              oi->op = Env::core_op("reference");
              note_change(oi);
    	  } else {
    		  V2 << "NULL Input in Restrict To Reference for " << ce2s(oi) << endl;
    	  }
      }
  }
};

// Map Stores to Reads
// TODO: Put this in the Read OperatorInstance, would need to extend OI to add a field and redo the below where we just change the delay to a read
// Need to map store and reads so we can get the right reference later in the emitter
std::map<OI*, OI*> readToStoreMap;

class DelayToStoreAndRead : public IRPropagator {
public:
  DelayToStoreAndRead(GlobalToLocal* parent, Args* args) : IRPropagator(false,true) {
    verbosity = args->extract_switch("--delay-to-store-and-read-verbosity") ?
    args->pop_int() : parent->verbosity;
  }
  virtual void print(ostream* out=0) { *out << "DelayToStoreAndRead"; }

  void act(OperatorInstance* oi) {
    if(oi->op==Env::core_op("delay")) {
      AM* space = oi->output->domain;
      V3 << "Converting delay to a store and a read: " << ce2s(oi) << endl;
      V3 << "Creating Store" << endl;
      OI* mux = oi->inputs[0]->producer;
      if (mux->op == Env::core_op("mux")) {
    	  V3 << "Changing the delay to a read" << endl;
    	  oi->op = Env::core_op("read");
    	  oi->output->range = mux->output->range;
    	  mux->output->unuse(oi, 1);
    	  oi->remove_input(0);

    	  OI* store = new OperatorInstance(mux, Env::core_op("store"), mux->output->domain);
       	  root->relocate_consumers(mux->output, store->output);
    	  store->add_input(mux->output);
    	  store->output->range = mux->output->range;
    	  store->clear_attribute("LETFED-MUX");

    	  readToStoreMap[oi] = store;

    	  note_change(oi);
    	  note_change(mux);
    	  note_change(store);
    	  V3 << "Mux: " << ce2s(mux) << endl;
    	  V3 << "Read: " << ce2s(oi) << endl;
    	  V3 << "Store: " << ce2s(store) << endl;
    	  V3 << "Mux output: " << ce2s(mux->output) << endl;
      } else {
    	  cout << "Delays input is not from a letfedmux!" << endl;
      }
    } else if(oi->op==Env::core_op("mux")) {
       // Handle the case where we have a dchange with no delay
       // This can happen if the update function ignores the rep variable
       //   So it's probably bad code
       //   may want to throw a compiler error instead?  But for now it's allowed so we have to handle it.
    	if (oi->attributes.count("LETFED-MUX")) {
    		Field* out = oi->output;
    		// If output goes to a delay, leave it alone here, the delay will handle it
    		for_set(Consumer,out->consumers,i) {
    		    if(i->first->op==Env::core_op("delay")) {
    		    	return;
    		    }
    		    if(i->first->op==Env::core_op("store")) {
    		    	// already did it
    		    	return;
    		    }
    		}
    		// No delay, and it's a letfed mux
    		// This is the silly case where there is no reference to the rep variable in the update
    		//   So the rep is really doing nothing (rep t 0 (+ 1 2))

    		// Add in a store, which will be emitted as a FeedbackOp for completeness and to match what the paleo compiler does
    		V3 << "Creating Store" << endl;
            OI* store = new OperatorInstance(oi, Env::core_op("store"), oi->output->domain);
         	root->relocate_consumers(oi->output, store->output);
      	    store->add_input(oi->output);
      	    store->output->range = oi->output->range;
      	    store->clear_attribute("LETFED-MUX");

      	    note_change(oi);
      	    note_change(store);
        	V3 << "Store: " << ce2s(store) << endl;
      	    V3 << "Mux output: " << ce2s(oi->output) << endl;
    	}
     }
  }
};


GlobalToLocal::GlobalToLocal(NeoCompiler* parent, Args* args) {
  verbosity=args->extract_switch("--localizer-verbosity") ? 
    args->pop_int() : parent->verbosity;
  max_loops=args->extract_switch("--localizer-max-loops")?args->pop_int():10;
  paranoid = args->extract_switch("--localizer-paranoid")|parent->paranoid;
  // set up rule collection
  rules.push_back(new HoodToFolder(this,args));
  rules.push_back(new RestrictToReference(this,args));
  rules.push_back(new RestrictToBranch(this,args));
  rules.push_back(new DelayToStoreAndRead(this, args));
  rules.push_back(new DeadCodeEliminator(this,args));
}

/* GOING GLOBAL TO LOCAL:
   Three transformations: restriction, feedback, neighborhood

   Hood: key question - how far should local computations reach?
     Proposal: all no-input-ops are stuck into a let via "local" op
     So... can identify subgraph that bounds inputs...
       put that subgraph into a new compound - second LAMBDA
       put all NBR ops into a tuple, which is used for the 3rd input
       <LAMBDA>, <LAMBDA>, <LOCAL> --> [fold-hood-plus] --> <LOCAL>
       lambda of a primitive op -> FUN_4_OP, REF_1_OP, REF_0_OP, <prim>, RET_OP
     What about fields used in two different operations?
       e.g. (let ((v (nbr x))) (- (min-hood v) (max-hood v)))
       that looks like two different exports...
 */

/*****************************************************************************
 *  GENERIC TRANSFORMATION CYCLER                                            *
 *****************************************************************************/

void DFGTransformer::transform(DFG* g) {
  CertifyBackpointers checker(verbosity);
  if(paranoid) checker.propagate(g); // make sure we're starting OK
  for(int i=0;i<max_loops;i++) {
    bool changed=false;
    for(int j=0;j<rules.size();j++) {
      changed |= rules[j]->propagate(g); terminate_on_error();
      if(paranoid) checker.propagate(g); // make sure we didn't break anything
    }
    if(!changed) break;
    if(i==(max_loops-1))
      compile_warn("Transformer giving up after "+i2s(max_loops)+" loops");
  }
  g->determine_relevant();
  checker.propagate(g); // make sure we didn't break anything
}

