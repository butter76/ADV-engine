// Showdown's handlers, ported one for one: a function here is the callback
// of the same effect and name in Showdown's data (the checklist of
// tools/dump_dex.cjs). A branch that needs something the engine does not
// have yet calls unported() and says what.

#pragma once

#include "events.hpp"

namespace adv {

inline void add(Ctx& c, std::initializer_list<Part> parts, bool secret_only = false) {
  Line line;
  for (const Part& p : parts) line.parts[line.n++] = p;
  line.secret_only = secret_only;
  c.out.log.push_back(line);
}

// Field.suppressingWeather (field.ts:106): Cloud Nine or Air Lock is out.
inline bool suppressing_weather(const Battle& b) {
  for (const Side& s : b.sides) {
    if (s.active == 0xFF) continue;
    const Pokemon& p = s.pokemon[s.active];
    if (!p.fainted && data(p.ability).suppress_weather) return true;
  }
  return false;
}
// Pokemon.effectiveWeather (pokemon.ts:2212) and Field.effectiveWeather.
inline Condition effective_weather(const Battle& b) {
  return suppressing_weather(b) ? Condition::None : b.field.weather;
}

namespace callbacks {

#define ADV_HANDLER(name) inline Relay name(Ctx& c, Relay value, Subject t, Ref source, Effect source_effect)
#define ADV_UNUSED (void)c, (void)value, (void)t, (void)source, (void)source_effect

// ---------------------------------------------------------------------------
// Rules (data/rulesets.ts). An onBegin writes the rule's line.
// ---------------------------------------------------------------------------
#define ADV_RULE_BEGIN(name, line)                \
  ADV_HANDLER(name) {                             \
    ADV_UNUSED;                                   \
    add(c, {part::text("rule"), part::text(line)}); \
    return relay::undefined;                      \
  }
ADV_HANDLER(hppercentagemod_onBegin) {
  ADV_UNUSED;
  add(c, {part::text("rule"), part::text("HP Percentage Mod: HP is shown in percentages")});
  c.b.report_percentages = true;
  return relay::undefined;
}
ADV_HANDLER(cancelmod_onBegin) {
  ADV_UNUSED;
  c.b.support_cancel = true;
  return relay::undefined;
}
ADV_RULE_BEGIN(beatupnicknamesmod_onBegin, "Beat Up Nicknames Mod: Beat Up will not reveal any party members")
ADV_RULE_BEGIN(endlessbattleclause_onBegin, "Endless Battle Clause: Forcing endless battles is banned")
ADV_RULE_BEGIN(sleepclausemod_onBegin, "Sleep Clause Mod: Limit one foe put to sleep")
ADV_RULE_BEGIN(switchpriorityclausemod_onBegin, "Switch Priority Clause Mod: Faster Pokemon switch first")
ADV_RULE_BEGIN(speciesclause_onBegin, "Species Clause: Limit one of each Pokemon")
ADV_RULE_BEGIN(ohkoclause_onBegin, "OHKO Clause: OHKO moves are banned")
ADV_RULE_BEGIN(evasionitemsclause_onBegin, "Evasion Items Clause: Evasion items are banned")
ADV_RULE_BEGIN(evasionmovesclause_onBegin, "Evasion Moves Clause: Evasion moves are banned")
ADV_RULE_BEGIN(oneboostpasserclause_onBegin,
               "One Boost Passer Clause: Limit one Baton Passer that has a way to boost its stats")
ADV_RULE_BEGIN(freezeclausemod_onBegin, "Freeze Clause Mod: Limit one foe frozen")
ADV_RULE_BEGIN(speedpassclause_onBegin, "Baton Pass Stat Clause: No Baton Passer may have a way to boost its Speed")

// ---------------------------------------------------------------------------
// Abilities (data/mods/gen3/abilities.ts, data/abilities.ts).
// ---------------------------------------------------------------------------
ADV_HANDLER(pressure_onStart) {
  ADV_UNUSED;
  // addSplit(side, ['-ability', pokemon, 'Pressure', '[silent]']): the other
  // side is sent an empty line.
  add(c, {part::text("-ability"), part::pokemon(c.b, t.pokemon), part::text("Pressure"), part::text("[silent]")}, true);
  return relay::undefined;
}
ADV_HANDLER(truant_onSwitchIn) {
  ADV_UNUSED;
  if (c.b.turn != 0) unported("ability truant onSwitchIn after turn 0");
  return relay::undefined; // truantTurn false; no volatile to remove
}
ADV_HANDLER(swiftswim_onModifySpe) {
  ADV_UNUSED;
  const Condition weather = effective_weather(c.b);
  if (weather == Condition::Raindance || weather == Condition::Primordialsea) {
    chain_modify(c, 2);
    return relay::undefined;
  }
  return relay::undefined;
}
ADV_HANDLER(chlorophyll_onModifySpe) {
  ADV_UNUSED;
  const Condition weather = effective_weather(c.b);
  if (weather == Condition::Sunnyday || weather == Condition::Desolateland) chain_modify(c, 2);
  return relay::undefined;
}
// The abilities that cure a status as soon as their Pokémon has it.
#define ADV_CURE_ON_UPDATE(name, cures, what)                             \
  ADV_HANDLER(name) {                                                     \
    ADV_UNUSED;                                                           \
    const Condition status = c.b.at(t.pokemon).status;                    \
    if (cures) unported(what " onUpdate with the status it cures");       \
    return relay::undefined;                                              \
  }
ADV_CURE_ON_UPDATE(insomnia_onUpdate, status == Condition::Slp, "ability insomnia")
ADV_CURE_ON_UPDATE(vitalspirit_onUpdate, status == Condition::Slp, "ability vitalspirit")
ADV_CURE_ON_UPDATE(immunity_onUpdate, status == Condition::Psn || status == Condition::Tox, "ability immunity")
ADV_CURE_ON_UPDATE(limber_onUpdate, status == Condition::Par, "ability limber")
ADV_CURE_ON_UPDATE(magmaarmor_onUpdate, status == Condition::Frz, "ability magmaarmor")
ADV_CURE_ON_UPDATE(waterveil_onUpdate, status == Condition::Brn, "ability waterveil")
// Oblivious and Own Tempo end a volatile; there are no volatiles yet.
ADV_CURE_ON_UPDATE(oblivious_onUpdate, (void(status), false), "ability oblivious")
ADV_CURE_ON_UPDATE(owntempo_onUpdate, (void(status), false), "ability owntempo")

// ---------------------------------------------------------------------------
// Items (data/items.ts, data/mods/gen3/items.ts). The berries eat themselves
// on the status they cure.
// ---------------------------------------------------------------------------
ADV_CURE_ON_UPDATE(aspearberry_onUpdate, status == Condition::Frz, "item aspearberry")
ADV_CURE_ON_UPDATE(cheriberry_onUpdate, status == Condition::Par, "item cheriberry")
ADV_CURE_ON_UPDATE(chestoberry_onUpdate, status == Condition::Slp, "item chestoberry")
ADV_CURE_ON_UPDATE(lumberry_onUpdate, status != Condition::None, "item lumberry")
ADV_CURE_ON_UPDATE(pechaberry_onUpdate, status == Condition::Psn || status == Condition::Tox, "item pechaberry")
ADV_CURE_ON_UPDATE(rawstberry_onUpdate, status == Condition::Brn, "item rawstberry")
ADV_CURE_ON_UPDATE(persimberry_onUpdate, (void(status), false), "item persimberry")
ADV_CURE_ON_UPDATE(mentalherb_onUpdate, (void(status), false), "item mentalherb")
ADV_HANDLER(leppaberry_onUpdate) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (!p.hp) return relay::undefined;
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].pp == 0) unported("item leppaberry onUpdate with a move out of PP");
  }
  return relay::undefined;
}
ADV_HANDLER(machobrace_onModifySpe) {
  ADV_UNUSED;
  chain_modify(c, 0.5);
  return relay::undefined;
}

// ---------------------------------------------------------------------------
// Conditions (data/conditions.ts, data/mods/gen3/conditions.ts).
// ---------------------------------------------------------------------------
ADV_HANDLER(par_onModifySpe) {
  ADV_UNUSED;
  chain_modify(c, 0.25); // Quick Feet is not Gen 3's
  return relay::undefined;
}

#undef ADV_HANDLER
#undef ADV_UNUSED
#undef ADV_RULE_BEGIN
#undef ADV_CURE_ON_UPDATE

inline constexpr Ported PORTED[] = {
#define R(rule, cb, fn) {effect(Rule::rule), Callback::cb, &fn}
#define A(ability, cb, fn) {effect(Ability::ability), Callback::cb, &fn}
#define I(item, cb, fn) {effect(Item::item), Callback::cb, &fn}
#define C(condition, cb, fn) {effect(Condition::condition), Callback::cb, &fn}
    R(HPPercentageMod, onBegin, hppercentagemod_onBegin),
    R(CancelMod, onBegin, cancelmod_onBegin),
    R(BeatUpNicknamesMod, onBegin, beatupnicknamesmod_onBegin),
    R(EndlessBattleClause, onBegin, endlessbattleclause_onBegin),
    R(SleepClauseMod, onBegin, sleepclausemod_onBegin),
    R(SwitchPriorityClauseMod, onBegin, switchpriorityclausemod_onBegin),
    R(SpeciesClause, onBegin, speciesclause_onBegin),
    R(OHKOClause, onBegin, ohkoclause_onBegin),
    R(EvasionItemsClause, onBegin, evasionitemsclause_onBegin),
    R(EvasionMovesClause, onBegin, evasionmovesclause_onBegin),
    R(OneBoostPasserClause, onBegin, oneboostpasserclause_onBegin),
    R(FreezeClauseMod, onBegin, freezeclausemod_onBegin),
    R(SpeedPassClause, onBegin, speedpassclause_onBegin),
    A(Pressure, onStart, pressure_onStart),
    A(Truant, onSwitchIn, truant_onSwitchIn),
    A(SwiftSwim, onModifySpe, swiftswim_onModifySpe),
    A(Chlorophyll, onModifySpe, chlorophyll_onModifySpe),
    A(Insomnia, onUpdate, insomnia_onUpdate),
    A(VitalSpirit, onUpdate, vitalspirit_onUpdate),
    A(Immunity, onUpdate, immunity_onUpdate),
    A(Limber, onUpdate, limber_onUpdate),
    A(MagmaArmor, onUpdate, magmaarmor_onUpdate),
    A(WaterVeil, onUpdate, waterveil_onUpdate),
    A(Oblivious, onUpdate, oblivious_onUpdate),
    A(OwnTempo, onUpdate, owntempo_onUpdate),
    I(AspearBerry, onUpdate, aspearberry_onUpdate),
    I(CheriBerry, onUpdate, cheriberry_onUpdate),
    I(ChestoBerry, onUpdate, chestoberry_onUpdate),
    I(LumBerry, onUpdate, lumberry_onUpdate),
    I(PechaBerry, onUpdate, pechaberry_onUpdate),
    I(RawstBerry, onUpdate, rawstberry_onUpdate),
    I(PersimBerry, onUpdate, persimberry_onUpdate),
    I(MentalHerb, onUpdate, mentalherb_onUpdate),
    I(LeppaBerry, onUpdate, leppaberry_onUpdate),
    I(MachoBrace, onModifySpe, machobrace_onModifySpe),
    C(Par, onModifySpe, par_onModifySpe),
#undef R
#undef A
#undef I
#undef C
};

} // namespace callbacks

inline std::span<const Ported> ported() { return callbacks::PORTED; }

} // namespace adv
