// Showdown's handlers, ported one for one: a function here is the callback
// of the same effect and name in Showdown's data (the checklist of
// tools/dump_dex.cjs). A branch that needs something the engine does not
// have yet calls unported() and says what.

#pragma once

#include "sim.hpp"

namespace adv {

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
  add_split(c, t.pokemon.side,
            {part::text("-ability"), part::pokemon(c.b, t.pokemon), part::text("Pressure"), part::text("[silent]")});
  return relay::undefined;
}
// Gen 3's weather abilities run their onStart from onAfterEntryHazard, which
// getCallback makes their switch-in handler at turn 0.
ADV_HANDLER(sandstream_onStart) {
  ADV_UNUSED;
  set_weather(c, Condition::Sandstorm);
  return relay::undefined;
}
ADV_HANDLER(sandstream_onAfterEntryHazard) { return sandstream_onStart(c, value, t, source, source_effect); }
ADV_HANDLER(intimidate_onStart) {
  ADV_UNUSED;
  // adjacentFoes(); no Pokémon has a Substitute yet.
  const Ref foe = foe_of(c.b, t.pokemon);
  if (!foe) {
    hint(c, "In Gen 3, Intimidate does not activate if every target has a Substitute.", t.pokemon.side);
    return relay::undefined;
  }
  add(c, {part::text("-ability"), part::pokemon(c.b, t.pokemon), part::text("Intimidate"), part::text("boost")});
  SparseBoosts drop;
  drop.set(0, -1); // atk
  boost(c, drop, foe, t.pokemon, true);
  return relay::undefined;
}
// The abilities that refuse a lowered stat: onTryBoost(boost, target, source,
// effect). The line is not written for a move's secondary effect.
#define ADV_KEEP_STATS(name, ability, first, last, shown)                                              \
  ADV_HANDLER(name) {                                                                                  \
    ADV_UNUSED;                                                                                        \
    if (source && t.pokemon == source) return relay::undefined;                                        \
    bool show = false;                                                                                 \
    for (int stat = first; stat <= last; stat++) {                                                     \
      if (value.boosts.has(stat) && value.boosts.value[size_t(stat)] < 0) value.boosts.remove(stat), show = true; \
    }                                                                                                  \
    const bool secondary = source_effect.kind == EffectKind::Move && data(Move(source_effect.id)).secondary.chance; \
    if (show && !secondary) {                                                                          \
      if (shown[0]) {                                                                                  \
        add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("unboost"), part::text(shown), \
                part::from_ability(Ability::ability), part::of(c.b, t.pokemon)});                      \
      } else {                                                                                         \
        add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("unboost"),              \
                part::from_ability(Ability::ability), part::of(c.b, t.pokemon)});                      \
      }                                                                                                \
    }                                                                                                  \
    return value;                                                                                      \
  }
ADV_KEEP_STATS(clearbody_onTryBoost, ClearBody, 0, BOOST_STATS - 1, "")
ADV_KEEP_STATS(whitesmoke_onTryBoost, WhiteSmoke, 0, BOOST_STATS - 1, "")
ADV_KEEP_STATS(hypercutter_onTryBoost, HyperCutter, 0, 0, "atk")
ADV_KEEP_STATS(keeneye_onTryBoost, KeenEye, 5, 5, "accuracy")
#undef ADV_KEEP_STATS

ADV_HANDLER(speedboost_onResidual) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).active_turns) {
    SparseBoosts up;
    up.set(4, 1); // spe
    boost(c, up, t.pokemon, {});
  }
  return relay::undefined;
}
ADV_HANDLER(shedskin_onResidual) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  // The draw is made only for a Pokémon with HP and a status.
  if (p.hp && p.status != Condition::None) unported("ability shedskin onResidual with a status");
  return relay::undefined;
}
ADV_HANDLER(naturalcure_onSwitchOut) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).status != Condition::None) unported("ability naturalcure onSwitchOut with a status");
  return relay::undefined;
}
ADV_HANDLER(rockhead_onDamage) {
  ADV_UNUSED;
  // Only recoil, which is not a condition the engine has yet, is refused.
  return relay::undefined;
}

// Trapping. `this.effectState.target` is the ability's holder.
ADV_HANDLER(arenatrap_onFoeTrapPokemon) {
  ADV_UNUSED;
  if (!is_adjacent(c.b, t.pokemon, c.effect_state->target_pokemon)) return relay::undefined;
  if (is_grounded(c.b.at(t.pokemon))) try_trap(c, t.pokemon, true);
  return relay::undefined;
}
ADV_HANDLER(arenatrap_onFoeMaybeTrapPokemon) {
  ADV_UNUSED;
  if (!source) source = c.effect_state->target_pokemon;
  if (!source || !is_adjacent(c.b, t.pokemon, source)) return relay::undefined;
  Pokemon& p = c.b.at(t.pokemon);
  if (is_grounded(p, !p.known_type)) p.maybe_trapped = true;
  return relay::undefined;
}
ADV_HANDLER(shadowtag_onFoeTrapPokemon) {
  ADV_UNUSED;
  c.b.at(t.pokemon).trapped = Trapped::Yes;
  return relay::undefined;
}
ADV_HANDLER(shadowtag_onFoeMaybeTrapPokemon) {
  ADV_UNUSED;
  if (!source) source = c.effect_state->target_pokemon;
  if (!source || !is_adjacent(c.b, t.pokemon, source)) return relay::undefined;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.ability != Ability::ShadowTag) p.maybe_trapped = true;
  return relay::undefined;
}
ADV_HANDLER(magnetpull_onAnyTrapPokemon) {
  ADV_UNUSED;
  if (has_type(c.b.at(t.pokemon), Type::Steel) && is_adjacent(c.b, t.pokemon, c.effect_state->target_pokemon)) {
    try_trap(c, t.pokemon, true);
  }
  return relay::undefined;
}
ADV_HANDLER(magnetpull_onAnyMaybeTrapPokemon) {
  ADV_UNUSED;
  if (!source) source = c.effect_state->target_pokemon;
  if (!source || !is_adjacent(c.b, t.pokemon, source)) return relay::undefined;
  Pokemon& p = c.b.at(t.pokemon);
  if (!p.known_type || has_type(p, Type::Steel)) p.maybe_trapped = true;
  return relay::undefined;
}
// onImmunity(type, pokemon): false makes the Pokémon immune to that.
ADV_HANDLER(sandveil_onImmunity) {
  ADV_UNUSED;
  return value.text == "sandstorm" ? relay::no : relay::undefined;
}
ADV_HANDLER(oblivious_onImmunity) {
  ADV_UNUSED;
  return value.text == "attract" ? relay::no : relay::undefined;
}
ADV_HANDLER(magmaarmor_onImmunity) {
  ADV_UNUSED;
  return value.text == "frz" ? relay::no : relay::undefined;
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
ADV_HANDLER(leftovers_onResidual) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 16);
  return relay::undefined;
}
// onDamage(damage, target, source, effect): the draw is made for every
// damage, whatever it is from.
ADV_HANDLER(focusband_onDamage) {
  ADV_UNUSED;
  const bool lucky = random(c.b, c.out, 10) < 1; // randomChance(1, 10)
  if (lucky && value.number >= c.b.at(t.pokemon).hp && source_effect.kind == EffectKind::Move && source_effect) {
    unported("item focusband saving its holder");
  }
  return relay::undefined;
}
// The berries eaten at the end of a turn at half or a quarter of full HP.
#define ADV_PINCH_BERRY(name, divisor, what)                                        \
  ADV_HANDLER(name) {                                                               \
    ADV_UNUSED;                                                                     \
    const Pokemon& p = c.b.at(t.pokemon);                                           \
    if (double(p.hp) <= double(p.maxhp) / divisor) unported(what " eaten");         \
    return relay::undefined;                                                        \
  }
ADV_PINCH_BERRY(aguavberry_onResidual, 2, "item aguavberry")
ADV_PINCH_BERRY(figyberry_onResidual, 2, "item figyberry")
ADV_PINCH_BERRY(iapapaberry_onResidual, 2, "item iapapaberry")
ADV_PINCH_BERRY(magoberry_onResidual, 2, "item magoberry")
ADV_PINCH_BERRY(oranberry_onResidual, 2, "item oranberry")
ADV_PINCH_BERRY(sitrusberry_onResidual, 2, "item sitrusberry")
ADV_PINCH_BERRY(wikiberry_onResidual, 2, "item wikiberry")
ADV_PINCH_BERRY(apicotberry_onResidual, 4, "item apicotberry")
ADV_PINCH_BERRY(ganlonberry_onResidual, 4, "item ganlonberry")
ADV_PINCH_BERRY(lansatberry_onResidual, 4, "item lansatberry")
ADV_PINCH_BERRY(liechiberry_onResidual, 4, "item liechiberry")
ADV_PINCH_BERRY(petayaberry_onResidual, 4, "item petayaberry")
ADV_PINCH_BERRY(salacberry_onResidual, 4, "item salacberry")
ADV_PINCH_BERRY(starfberry_onResidual, 4, "item starfberry")
#undef ADV_PINCH_BERRY
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
// Sandstorm. Smooth Rock is not Gen 3's, so it lasts 5 turns; set by an
// ability it has no duration at all.
ADV_HANDLER(sandstorm_durationCallback) {
  ADV_UNUSED;
  return relay::number(5);
}
ADV_HANDLER(sandstorm_onFieldStart) {
  ADV_UNUSED;
  if (source_effect.kind == EffectKind::Ability && source_effect) {
    c.effect_state->duration = 0;
    add(c, {part::text("-weather"), part::text("Sandstorm"), part::from_ability(Ability(source_effect.id)),
            part::of(c.b, source)});
  } else {
    add(c, {part::text("-weather"), part::text("Sandstorm")});
  }
  return relay::undefined;
}
ADV_HANDLER(sandstorm_onFieldResidual) {
  ADV_UNUSED;
  add(c, {part::text("-weather"), part::text("Sandstorm"), part::text("[upkeep]")});
  if (effective_weather(c.b) == Condition::Sandstorm) each_event(c, WEATHER);
  return relay::undefined;
}
ADV_HANDLER(sandstorm_onWeather) {
  ADV_UNUSED;
  damage(c, double(c.b.at(t.pokemon).base_maxhp) / 16);
  return relay::undefined;
}
ADV_HANDLER(sunnyday_onImmunity) {
  ADV_UNUSED;
  if (effective_weather(c.b) != Condition::Sunnyday) return relay::undefined;
  return value.text == "frz" ? relay::no : relay::undefined;
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
    A(SandStream, onStart, sandstream_onStart),
    A(SandStream, onAfterEntryHazard, sandstream_onAfterEntryHazard),
    A(Intimidate, onStart, intimidate_onStart),
    A(ClearBody, onTryBoost, clearbody_onTryBoost),
    A(WhiteSmoke, onTryBoost, whitesmoke_onTryBoost),
    A(HyperCutter, onTryBoost, hypercutter_onTryBoost),
    A(KeenEye, onTryBoost, keeneye_onTryBoost),
    A(ArenaTrap, onFoeTrapPokemon, arenatrap_onFoeTrapPokemon),
    A(ArenaTrap, onFoeMaybeTrapPokemon, arenatrap_onFoeMaybeTrapPokemon),
    A(ShadowTag, onFoeTrapPokemon, shadowtag_onFoeTrapPokemon),
    A(ShadowTag, onFoeMaybeTrapPokemon, shadowtag_onFoeMaybeTrapPokemon),
    A(MagnetPull, onAnyTrapPokemon, magnetpull_onAnyTrapPokemon),
    A(MagnetPull, onAnyMaybeTrapPokemon, magnetpull_onAnyMaybeTrapPokemon),
    A(SandVeil, onImmunity, sandveil_onImmunity),
    A(SpeedBoost, onResidual, speedboost_onResidual),
    A(ShedSkin, onResidual, shedskin_onResidual),
    A(NaturalCure, onSwitchOut, naturalcure_onSwitchOut),
    A(RockHead, onDamage, rockhead_onDamage),
    A(Oblivious, onImmunity, oblivious_onImmunity),
    A(MagmaArmor, onImmunity, magmaarmor_onImmunity),
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
    I(Leftovers, onResidual, leftovers_onResidual),
    I(FocusBand, onDamage, focusband_onDamage),
    I(AguavBerry, onResidual, aguavberry_onResidual),
    I(FigyBerry, onResidual, figyberry_onResidual),
    I(IapapaBerry, onResidual, iapapaberry_onResidual),
    I(MagoBerry, onResidual, magoberry_onResidual),
    I(OranBerry, onResidual, oranberry_onResidual),
    I(SitrusBerry, onResidual, sitrusberry_onResidual),
    I(WikiBerry, onResidual, wikiberry_onResidual),
    I(ApicotBerry, onResidual, apicotberry_onResidual),
    I(GanlonBerry, onResidual, ganlonberry_onResidual),
    I(LansatBerry, onResidual, lansatberry_onResidual),
    I(LiechiBerry, onResidual, liechiberry_onResidual),
    I(PetayaBerry, onResidual, petayaberry_onResidual),
    I(SalacBerry, onResidual, salacberry_onResidual),
    I(StarfBerry, onResidual, starfberry_onResidual),
    C(Par, onModifySpe, par_onModifySpe),
    C(Sandstorm, durationCallback, sandstorm_durationCallback),
    C(Sandstorm, onFieldStart, sandstorm_onFieldStart),
    C(Sandstorm, onFieldResidual, sandstorm_onFieldResidual),
    C(Sandstorm, onWeather, sandstorm_onWeather),
    C(Sunnyday, onImmunity, sunnyday_onImmunity),
#undef R
#undef A
#undef I
#undef C
};

} // namespace callbacks

inline std::span<const Ported> ported() { return callbacks::PORTED; }

} // namespace adv
