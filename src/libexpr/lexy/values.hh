#pragma once
/**
 * Values for parser callbacks and native builders. Expression handles refer
 * to native nodes; unfinished lists, bindings, formals and literal fragments
 * use scratch storage. Builders must copy or intern borrowed text and consume
 * scratch containers before their storage is released.
 */
#include "values/kinds.hh"
#include "values/expression.hh"
#include "values/parts.hh"
#include "values/paths.hh"
#include "values/attributes.hh"
#include "values/lambdas.hh"
