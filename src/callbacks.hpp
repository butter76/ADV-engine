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
  if (p.hp && p.status != Condition::None && random(c.b, c.out, 100) < 33) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("ability: Shed Skin")});
    cure_status(c, t.pokemon);
  }
  return relay::undefined;
}
ADV_HANDLER(naturalcure_onSwitchOut) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (p.status == Condition::None || p.status_fnt) return relay::undefined;
  add(c, {part::text("-curestatus"), part::pokemon(c.b, t.pokemon), part::text(CONDITION_NAMES[uint8_t(p.status)].id),
          part::from(effect(Ability::NaturalCure)), part::text("[silent]")});
  clear_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(rockhead_onDamage) {
  ADV_UNUSED;
  // Recoil is refused, but for Struggle's.
  if (source_effect == effect(Special::Recoil) && c.b.active_move.id != Move::Struggle) return relay::null;
  return relay::undefined;
}
ADV_HANDLER(liquidooze_onSourceTryHeal) {
  ADV_UNUSED;
  if (source_effect == effect(Special::Drain) || source_effect == effect(Move::LeechSeed)) {
    unported("ability liquidooze against a draining move");
  }
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
#define ADV_CURE_ON_UPDATE(name, cures, text_)                                              \
  ADV_HANDLER(name) {                                                                       \
    ADV_UNUSED;                                                                             \
    const Condition status = c.b.at(t.pokemon).status;                                      \
    if (cures) {                                                                            \
      add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text(text_)});  \
      cure_status(c, t.pokemon);                                                            \
    }                                                                                       \
    return relay::undefined;                                                                \
  }
ADV_CURE_ON_UPDATE(insomnia_onUpdate, status == Condition::Slp, "ability: Insomnia")
ADV_CURE_ON_UPDATE(vitalspirit_onUpdate, status == Condition::Slp, "ability: Vital Spirit")
ADV_CURE_ON_UPDATE(immunity_onUpdate, status == Condition::Psn || status == Condition::Tox, "ability: Immunity")
ADV_CURE_ON_UPDATE(limber_onUpdate, status == Condition::Par, "ability: Limber")
ADV_CURE_ON_UPDATE(magmaarmor_onUpdate, status == Condition::Frz, "ability: Magma Armor")
ADV_CURE_ON_UPDATE(waterveil_onUpdate, status == Condition::Brn, "ability: Water Veil")
#undef ADV_CURE_ON_UPDATE
// Oblivious and Own Tempo end a volatile that is not ported yet.
ADV_HANDLER(oblivious_onUpdate) {
  ADV_UNUSED;
  if (volatile_of(c.b.at(t.pokemon), Condition::Attract)) unported("ability oblivious ending Attract");
  return relay::undefined;
}
ADV_HANDLER(owntempo_onUpdate) {
  ADV_UNUSED;
  if (volatile_of(c.b.at(t.pokemon), Condition::Confusion)) unported("ability owntempo ending confusion");
  return relay::undefined;
}
// onSetStatus(status, target, source, effect): the status is refused, with a
// line when a status move tried it.
#define ADV_REFUSE_STATUS(name, ability, refuses)                                                       \
  ADV_HANDLER(name) {                                                                                   \
    ADV_UNUSED;                                                                                         \
    const std::string_view status = value.text;                                                         \
    if (!(refuses)) return relay::undefined;                                                            \
    if (source_effect.kind == EffectKind::Move && source_effect && data(Move(source_effect.id)).status != Condition::None) { \
      add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::ability))}); \
    }                                                                                                   \
    return relay::no;                                                                                   \
  }
ADV_REFUSE_STATUS(insomnia_onSetStatus, Insomnia, status == "slp")
ADV_REFUSE_STATUS(vitalspirit_onSetStatus, VitalSpirit, status == "slp")
ADV_REFUSE_STATUS(immunity_onSetStatus, Immunity, status == "psn" || status == "tox")
ADV_REFUSE_STATUS(limber_onSetStatus, Limber, status == "par")
ADV_REFUSE_STATUS(waterveil_onSetStatus, WaterVeil, status == "brn")
#undef ADV_REFUSE_STATUS
inline Condition status_of(std::string_view id) {
  for (const Condition status : {Condition::Slp, Condition::Frz, Condition::Brn, Condition::Par, Condition::Psn, Condition::Tox}) {
    if (CONDITION_NAMES[uint8_t(status)].id == id) return status;
  }
  return Condition::None;
}
ADV_HANDLER(synchronize_onAfterSetStatus) {
  ADV_UNUSED;
  if (!source || source == t.pokemon) return relay::undefined;
  Condition status = status_of(value.text);
  if (status == Condition::Slp || status == Condition::Frz) return relay::undefined;
  if (status == Condition::Tox) status = Condition::Psn;
  try_set_status(c, source, status, t.pokemon);
  return relay::undefined;
}
// onModifySecondaries: only the effects with a `self` are kept. The engine
// has one secondary a move, so "false" stands for the filtered list.
ADV_HANDLER(shielddust_onModifySecondaries) {
  ADV_UNUSED;
  return relay::no;
}
ADV_HANDLER(innerfocus_onTryAddVolatile) {
  ADV_UNUSED;
  return value.text == "flinch" ? relay::null : relay::undefined;
}
ADV_HANDLER(owntempo_onTryAddVolatile) {
  ADV_UNUSED;
  return value.text == "confusion" ? relay::null : relay::undefined;
}
ADV_HANDLER(insomnia_onTryAddVolatile) {
  ADV_UNUSED;
  if (value.text == "yawn") unported("ability insomnia against Yawn");
  return relay::undefined;
}
ADV_HANDLER(vitalspirit_onTryAddVolatile) {
  ADV_UNUSED;
  if (value.text == "yawn") unported("ability vitalspirit against Yawn");
  return relay::undefined;
}

// ---------------------------------------------------------------------------
// Items (data/items.ts, data/mods/gen3/items.ts). The berries eat themselves
// on the status they cure.
// ---------------------------------------------------------------------------
// Eating a berry is not ported.
#define ADV_BERRY_ON_UPDATE(name, cures, what)                          \
  ADV_HANDLER(name) {                                                   \
    ADV_UNUSED;                                                         \
    const Condition status = c.b.at(t.pokemon).status;                  \
    if (cures) unported(what " eaten");                                 \
    return relay::undefined;                                            \
  }
ADV_BERRY_ON_UPDATE(aspearberry_onUpdate, status == Condition::Frz, "item aspearberry")
ADV_BERRY_ON_UPDATE(cheriberry_onUpdate, status == Condition::Par, "item cheriberry")
ADV_BERRY_ON_UPDATE(chestoberry_onUpdate, status == Condition::Slp, "item chestoberry")
ADV_BERRY_ON_UPDATE(lumberry_onUpdate, status != Condition::None, "item lumberry")
ADV_BERRY_ON_UPDATE(pechaberry_onUpdate, status == Condition::Psn || status == Condition::Tox, "item pechaberry")
ADV_BERRY_ON_UPDATE(rawstberry_onUpdate, status == Condition::Brn, "item rawstberry")
ADV_BERRY_ON_UPDATE(persimberry_onUpdate, (void(status), false), "item persimberry")
ADV_BERRY_ON_UPDATE(mentalherb_onUpdate, (void(status), false), "item mentalherb")
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
// onStart(target, source, sourceEffect) of a status: its line, which names
// the ability that gave it.
inline void status_line(Ctx& c, Ref target, Ref source, Effect source_effect, std::string_view status) {
  if (source_effect.kind == EffectKind::Ability && source_effect) {
    add(c, {part::text("-status"), part::pokemon(c.b, target), part::text(status), part::from(source_effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-status"), part::pokemon(c.b, target), part::text(status)});
  }
}
#define ADV_STATUS_START(name, status)                          \
  ADV_HANDLER(name) {                                           \
    ADV_UNUSED;                                                 \
    status_line(c, t.pokemon, source, source_effect, status);   \
    return relay::undefined;                                    \
  }
ADV_STATUS_START(par_onStart, "par")
ADV_STATUS_START(brn_onStart, "brn")
ADV_STATUS_START(psn_onStart, "psn")
ADV_STATUS_START(frz_onStart, "frz")
#undef ADV_STATUS_START
ADV_HANDLER(tox_onStart) {
  ADV_UNUSED;
  c.effect_state->stage = 0;
  status_line(c, t.pokemon, source, source_effect, "tox");
  return relay::undefined;
}
ADV_HANDLER(tox_onSwitchIn) {
  ADV_UNUSED;
  c.effect_state->stage = 0;
  return relay::undefined;
}
ADV_HANDLER(slp_onStart) {
  ADV_UNUSED;
  if (source_effect.kind == EffectKind::Move && source_effect) {
    add(c, {part::text("-status"), part::pokemon(c.b, t.pokemon), part::text("slp"), part::from(source_effect)});
  } else {
    add(c, {part::text("-status"), part::pokemon(c.b, t.pokemon), part::text("slp")});
  }
  c.effect_state->time = int8_t(random(c.b, c.out, 2, 6));
  c.effect_state->skipped_time = 0;
  return relay::undefined; // Nightmare, which it would end, is not ported
}
ADV_HANDLER(slp_onSwitchOut) {
  ADV_UNUSED;
  c.effect_state->time = int8_t(c.effect_state->time + c.effect_state->skipped_time);
  c.effect_state->skipped_time = 0;
  return relay::undefined;
}
ADV_HANDLER(slp_onBeforeMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.ability == Ability::EarlyBird) p.status_state.time--;
  p.status_state.time--;
  if (p.status_state.time <= 0) {
    cure_status(c, t.pokemon);
    return relay::undefined;
  }
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("slp")});
  if (data(Move(source_effect.id)).sleep_usable) {
    c.effect_state->skipped_time++;
    return relay::undefined;
  }
  c.effect_state->skipped_time = 0;
  return relay::no;
}
ADV_HANDLER(par_onBeforeMove) {
  ADV_UNUSED;
  if (random(c.b, c.out, 4) < 1) { // randomChance(1, 4)
    add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("par")});
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(frz_onBeforeMove) {
  ADV_UNUSED;
  if (random(c.b, c.out, 5) < 1) { // randomChance(1, 5)
    cure_status(c, t.pokemon);
    return relay::undefined;
  }
  if (data(Move(source_effect.id)).flags & flag(MoveFlag::Defrost)) return relay::undefined;
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("frz")});
  return relay::no;
}
ADV_HANDLER(frz_onModifyMove) {
  ADV_UNUSED;
  if (data(c.b.active_move.id).flags & flag(MoveFlag::Defrost)) {
    add(c, {part::text("-curestatus"), part::pokemon(c.b, t.pokemon), part::text("frz"), part::from(effect(c.b.active_move.id))});
    clear_status(c, t.pokemon);
  }
  return relay::undefined;
}
ADV_HANDLER(frz_onAfterMoveSecondary) {
  ADV_UNUSED;
  return relay::undefined; // no move of Gen 3 has thawsTarget
}
ADV_HANDLER(frz_onDamagingHit) {
  ADV_UNUSED;
  // The move's own type, not what it was changed to.
  const MoveData& m = data(Move(source_effect.id));
  if (m.type == Type::Fire && c.b.active_move.category != Category::Status) cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(brn_onResidual) {
  ADV_UNUSED;
  damage(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  return relay::undefined;
}
ADV_HANDLER(psn_onResidual) {
  ADV_UNUSED;
  damage(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  return relay::undefined;
}
ADV_HANDLER(tox_onResidual) {
  ADV_UNUSED;
  if (c.effect_state->stage < 15) c.effect_state->stage++;
  const double sixteenth = std::max(std::floor(double(c.b.at(t.pokemon).base_maxhp) / 16), 1.0);
  damage(c, sixteenth * c.effect_state->stage);
  return relay::undefined;
}
ADV_HANDLER(flinch_onBeforeMove) {
  ADV_UNUSED;
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("flinch")});
  return relay::no; // nothing listens for Flinch in Gen 3
}

// ---------------------------------------------------------------------------
// Moves (data/moves.ts, data/mods/gen3/moves.ts).
// ---------------------------------------------------------------------------
ADV_HANDLER(rest_onTry) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (p.status == Condition::Slp) return relay::no;
  if (p.hp == p.maxhp) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("heal")});
    return relay::null;
  }
  if (p.ability == Ability::Insomnia || p.ability == Ability::VitalSpirit) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::from(effect(p.ability)), part::of(c.b, t.pokemon)});
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(rest_onHit) {
  ADV_UNUSED;
  if (!set_status(c, t.pokemon, Condition::Slp, source, effect(Move::Rest))) return relay::no;
  Pokemon& p = c.b.at(t.pokemon);
  p.status_state.time = 3;
  heal(c, p.maxhp);
  return relay::undefined;
}
ADV_HANDLER(refresh_onHit) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status == Condition::None || status == Condition::Slp || status == Condition::Frz) return relay::no;
  cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(thunder_onModifyMove) {
  ADV_UNUSED;
  const Condition weather = effective_weather(c.b);
  if (weather == Condition::Raindance || weather == Condition::Primordialsea) c.b.active_move.accuracy = 0;
  if (weather == Condition::Sunnyday || weather == Condition::Desolateland) c.b.active_move.accuracy = 50;
  return relay::undefined;
}
// Curse from a Pokémon that is no Ghost raises its own stats. Both handlers
// set that, and the second lists Speed first.
ADV_HANDLER(curse_onModifyMove) {
  ADV_UNUSED;
  Battle::ActiveMove& move = c.b.active_move;
  if (has_type(c.b.at(t.pokemon), Type::Ghost)) return relay::undefined; // a Substitute would matter here
  move.volatile_status = Condition::None;
  move.on_hit = false;
  move.self_boosts = {.atk = 1, .def = 1, .spe = -1};
  move.target = Target::Self;
  return relay::undefined;
}
ADV_HANDLER(curse_onTryHit) {
  ADV_UNUSED;
  Battle::ActiveMove& move = c.b.active_move;
  if (!has_type(c.b.at(source), Type::Ghost)) {
    move.volatile_status = Condition::None;
    move.on_hit = false;
    move.self_boosts = {.atk = 1, .def = 1, .spe = -1};
    move.self_boosts_first = 4; // spe
  } else if (move.volatile_status != Condition::None && volatile_of(c.b.at(t.pokemon), Condition::Curse)) {
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(hiddenpower_onModifyMove) {
  ADV_UNUSED;
  Battle::ActiveMove& move = c.b.active_move;
  move.type = c.b.at(t.pokemon).hp_type;
  // Gen 3's special types.
  const Type type = move.type;
  const bool special = type == Type::Fire || type == Type::Water || type == Type::Grass || type == Type::Ice ||
                       type == Type::Electric || type == Type::Dark || type == Type::Psychic || type == Type::Dragon;
  move.category = special ? Category::Special : Category::Physical;
  return relay::undefined;
}
ADV_HANDLER(hiddenpower_onModifyType) {
  ADV_UNUSED;
  c.b.active_move.type = c.b.at(t.pokemon).hp_type;
  return relay::undefined;
}
ADV_HANDLER(hiddenpower_basePowerCallback) {
  ADV_UNUSED;
  return relay::number(c.b.at(t.pokemon).hp_power);
}
ADV_HANDLER(return_basePowerCallback) {
  ADV_UNUSED;
  return relay::number(std::max(c.b.at(t.pokemon).happiness * 10 / 25, 1));
}
ADV_HANDLER(frustration_basePowerCallback) {
  ADV_UNUSED;
  return relay::number(std::max((255 - c.b.at(t.pokemon).happiness) * 10 / 25, 1));
}

// ---------------------------------------------------------------------------
// The clauses that refuse a status (data/rulesets.ts).
// ---------------------------------------------------------------------------
ADV_HANDLER(sleepclausemod_onSetStatus) {
  ADV_UNUSED;
  if (source && source.side == t.pokemon.side) return relay::undefined;
  if (value.text != "slp") return relay::undefined;
  const Side& side = c.b.sides[t.pokemon.side];
  for (int position = 0; position < side.size; position++) {
    const Pokemon& p = side.pokemon[index_at(side, position)];
    if (!p.hp || p.status != Condition::Slp) continue;
    // Asleep by a foe's doing.
    if (p.status_state.source && p.status_state.source.side == t.pokemon.side) continue;
    add(c, {part::text("-message"), part::text("Sleep Clause Mod activated.")});
    add(c, {part::text("-hint"), part::text("Sleep Clause Mod prevents players from putting more than one of their opponent's Pokemon to sleep at a time")});
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(freezeclausemod_onSetStatus) {
  ADV_UNUSED;
  if (source && source.side == t.pokemon.side) return relay::undefined;
  if (value.text != "frz") return relay::undefined;
  const Side& side = c.b.sides[t.pokemon.side];
  for (int position = 0; position < side.size; position++) {
    if (side.pokemon[index_at(side, position)].status != Condition::Frz) continue;
    add(c, {part::text("-message"), part::text("Freeze Clause activated.")});
    return relay::no;
  }
  return relay::undefined;
}

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
ADV_HANDLER(sandstorm_onFieldEnd) {
  ADV_UNUSED;
  add(c, {part::text("-weather"), part::text("none")});
  return relay::undefined;
}
// Rain, sun and hail (data/conditions.ts). No rock lengthens them in Gen 3.
#define ADV_WEATHER(id, shown)                                                                       \
  ADV_HANDLER(id##_durationCallback) {                                                               \
    ADV_UNUSED;                                                                                      \
    return relay::number(5);                                                                         \
  }                                                                                                  \
  ADV_HANDLER(id##_onFieldStart) {                                                                   \
    ADV_UNUSED;                                                                                      \
    if (source_effect.kind == EffectKind::Ability && source_effect) {                                \
      c.effect_state->duration = 0;                                                                  \
      add(c, {part::text("-weather"), part::text(shown), part::from(source_effect), part::of(c.b, source)}); \
    } else {                                                                                         \
      add(c, {part::text("-weather"), part::text(shown)});                                           \
    }                                                                                                \
    return relay::undefined;                                                                         \
  }                                                                                                  \
  ADV_HANDLER(id##_onFieldEnd) {                                                                     \
    ADV_UNUSED;                                                                                      \
    add(c, {part::text("-weather"), part::text("none")});                                            \
    return relay::undefined;                                                                         \
  }
ADV_WEATHER(raindance, "RainDance")
ADV_WEATHER(sunnyday, "SunnyDay")
ADV_WEATHER(hail, "Hail")
#undef ADV_WEATHER
ADV_HANDLER(raindance_onFieldResidual) {
  ADV_UNUSED;
  add(c, {part::text("-weather"), part::text("RainDance"), part::text("[upkeep]")});
  each_event(c, WEATHER);
  return relay::undefined;
}
ADV_HANDLER(sunnyday_onFieldResidual) {
  ADV_UNUSED;
  add(c, {part::text("-weather"), part::text("SunnyDay"), part::text("[upkeep]")});
  each_event(c, WEATHER);
  return relay::undefined;
}
ADV_HANDLER(hail_onFieldResidual) {
  ADV_UNUSED;
  add(c, {part::text("-weather"), part::text("Hail"), part::text("[upkeep]")});
  if (effective_weather(c.b) == Condition::Hail) each_event(c, WEATHER);
  return relay::undefined;
}
ADV_HANDLER(hail_onWeather) {
  ADV_UNUSED;
  damage(c, double(c.b.at(t.pokemon).base_maxhp) / 16);
  return relay::undefined;
}
// onWeatherModifyDamage(damage, attacker, defender, move).
ADV_HANDLER(raindance_onWeatherModifyDamage) {
  ADV_UNUSED;
  if (effective_weather(c.b) != Condition::Raindance) return relay::undefined;
  const Type type = c.b.active_move.type;
  if (type == Type::Water) chain_modify(c, 1.5);
  else if (type == Type::Fire) chain_modify(c, 0.5);
  return relay::undefined;
}
ADV_HANDLER(sunnyday_onWeatherModifyDamage) {
  ADV_UNUSED;
  if (effective_weather(c.b) != Condition::Sunnyday) return relay::undefined;
  const Type type = c.b.active_move.type;
  if (type == Type::Fire) chain_modify(c, 1.5);
  else if (type == Type::Water) chain_modify(c, 0.5);
  return relay::undefined;
}
ADV_HANDLER(sunnyday_onImmunity) {
  ADV_UNUSED;
  if (effective_weather(c.b) != Condition::Sunnyday) return relay::undefined;
  return value.text == "frz" ? relay::no : relay::undefined;
}


// ---------------------------------------------------------------------------
// What a move's use and its hit run.
// ---------------------------------------------------------------------------

// The type of the move a handler is given (its `move` argument).
inline Type move_type(const Ctx& c, Effect move) {
  return move.kind == EffectKind::Move && move ? c.b.active_move.type : Type::None;
}
// A handler that is `if (condition) return this.chainModify(n, d)`.
#define ADV_CHAIN(name, condition, numerator, denominator)       \
  ADV_HANDLER(name) {                                            \
    ADV_UNUSED;                                                  \
    if (condition) chain_modify(c, numerator, denominator);      \
    return relay::undefined;                                     \
  }

// Abilities.
ADV_HANDLER(pressure_onDeductPP) {
  ADV_UNUSED;
  return t.pokemon == source ? relay::undefined : relay::number(1);
}
ADV_HANDLER(hustle_onModifyAtk) {
  ADV_UNUSED;
  return relay::number(modify(value.number, 1.5));
}
ADV_HANDLER(hustle_onSourceModifyAccuracy) {
  ADV_UNUSED;
  // Gen 3's physical types.
  const Type type = move_type(c, source_effect);
  const bool physical = type == Type::Normal || type == Type::Fighting || type == Type::Flying || type == Type::Poison ||
                        type == Type::Ground || type == Type::Rock || type == Type::Bug || type == Type::Ghost ||
                        type == Type::Steel;
  if (physical && value.kind == Relay::Kind::Number) chain_modify(c, 3277, 4096);
  return relay::undefined;
}
inline bool active_with(const Battle& b, Ability ability) {
  for (const Side& side : b.sides) {
    if (side.active != 0xFF && !side.pokemon[side.active].fainted && side.pokemon[side.active].ability == ability) return true;
  }
  return false;
}
ADV_CHAIN(minus_onModifySpA, active_with(c.b, Ability::Plus), 1.5, 1)
ADV_CHAIN(plus_onModifySpA, active_with(c.b, Ability::Minus), 1.5, 1)
// onBasePower(basePower, attacker, defender, move) at a third of full HP.
#define ADV_PINCH(name, type_)                                                                        \
  ADV_CHAIN(name, move_type(c, source_effect) == Type::type_ &&                                       \
                      double(c.b.at(t.pokemon).hp) <= double(c.b.at(t.pokemon).maxhp) / 3, 1.5, 1)
ADV_PINCH(blaze_onBasePower, Fire)
ADV_PINCH(overgrow_onBasePower, Grass)
ADV_PINCH(swarm_onBasePower, Bug)
ADV_PINCH(torrent_onBasePower, Water)
#undef ADV_PINCH
ADV_CHAIN(thickfat_onSourceBasePower, move_type(c, source_effect) == Type::Ice || move_type(c, source_effect) == Type::Fire, 0.5, 1)
ADV_CHAIN(compoundeyes_onSourceModifyAccuracy, value.kind == Relay::Kind::Number, 1.3, 1)
ADV_CHAIN(sandveil_onModifyAccuracy, value.kind == Relay::Kind::Number && effective_weather(c.b) == Condition::Sandstorm, 0.8, 1)
ADV_CHAIN(guts_onModifyAtk, c.b.at(t.pokemon).status != Condition::None, 1.5, 1)
ADV_CHAIN(hugepower_onModifyAtk, true, 2, 1)
ADV_CHAIN(purepower_onModifyAtk, true, 2, 1)
ADV_CHAIN(marvelscale_onModifyDef, c.b.at(t.pokemon).status != Condition::None, 1.5, 1)
// onCriticalHit: false, a constant.
ADV_HANDLER(battlearmor_onCriticalHit) {
  ADV_UNUSED;
  return relay::no;
}
ADV_HANDLER(shellarmor_onCriticalHit) {
  ADV_UNUSED;
  return relay::no;
}
// onTryHit(target, source, move).
ADV_HANDLER(sturdy_onTryHit) {
  ADV_UNUSED;
  if (data(Move(source_effect.id)).ohko) unported("ability sturdy against a one-hit KO move");
  return relay::undefined;
}
#define ADV_ABSORB(name, ability, type_, except)                                                        \
  ADV_HANDLER(name) {                                                                                   \
    ADV_UNUSED;                                                                                         \
    if (t.pokemon == source || move_type(c, source_effect) != Type::type_ || Move(source_effect.id) == except) { \
      return relay::undefined;                                                                          \
    }                                                                                                   \
    if (!heal(c, double(c.b.at(t.pokemon).base_maxhp) / 4)) {                                           \
      add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::ability))}); \
    }                                                                                                   \
    return relay::null;                                                                                 \
  }
ADV_ABSORB(voltabsorb_onTryHit, VoltAbsorb, Electric, Move::ThunderWave)
ADV_ABSORB(waterabsorb_onTryHit, WaterAbsorb, Water, Move::None)
#undef ADV_ABSORB
ADV_HANDLER(soundproof_onTryHit) {
  ADV_UNUSED;
  if (!(data(Move(source_effect.id)).flags & flag(MoveFlag::Sound))) return relay::undefined;
  add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::Soundproof))});
  return relay::null;
}
ADV_HANDLER(oblivious_onTryHit) {
  ADV_UNUSED;
  return relay::undefined; // only Captivate, which is not Gen 3's
}
ADV_HANDLER(wonderguard_onTryHit) {
  ADV_UNUSED;
  const Battle::ActiveMove& move = c.b.active_move;
  if (t.pokemon == source || move.category == Category::Status || move.type == Type::Unknown) return relay::undefined;
  // runEffectiveness and runImmunity of the move against the holder.
  const Pokemon& p = c.b.at(t.pokemon);
  int total = 0;
  bool immune_to = false;
  for (const Type type : p.types) {
    if (type == Type::None) continue;
    const uint8_t taken = damage_taken(type, move.type);
    total += taken == 1 ? 1 : taken == 2 ? -1 : 0;
    immune_to |= taken == 3;
  }
  if (move.type == Type::Ground) unported("ability wonderguard against a Ground move");
  if (total <= 0 || immune_to) {
    add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::WonderGuard))});
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(flashfire_onTryHit) {
  ADV_UNUSED;
  if (t.pokemon == source || move_type(c, source_effect) != Type::Fire) return relay::undefined;
  if (Move(source_effect.id) == Move::WillOWisp) unported("ability flashfire against Will-O-Wisp");
  if (c.b.at(t.pokemon).status == Condition::Frz) return relay::undefined;
  if (!add_volatile(c, t.pokemon, Condition::FlashFire)) {
    add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::FlashFire))});
  }
  return relay::null;
}
ADV_HANDLER(flashfire_onEnd) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::FlashFire);
  return relay::undefined;
}
ADV_HANDLER(flashfire_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("ability: Flash Fire")});
  return relay::undefined;
}
ADV_HANDLER(flashfire_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("ability: Flash Fire"), part::text("[silent]")});
  return relay::undefined;
}
ADV_CHAIN(flashfire_condition_onModifyDamagePhase1, move_type(c, source_effect) == Type::Fire, 1.5, 1)
// onDamagingHit(damage, target, source, move): what touching the holder does.
inline bool made_contact(Relay damage, Effect move) {
  return damage.number != 0 && (data(Move(move.id)).flags & flag(MoveFlag::Contact));
}
ADV_HANDLER(roughskin_onDamagingHit) {
  ADV_UNUSED;
  if (made_contact(value, source_effect)) damage(c, double(c.b.at(source).base_maxhp) / 16, source, t.pokemon);
  return relay::undefined;
}
// The draw is made on every contact; what it then does needs statuses.
#define ADV_CONTACT_STATUS(name, what)                                                  \
  ADV_HANDLER(name) {                                                                   \
    ADV_UNUSED;                                                                         \
    if (made_contact(value, source_effect) && random(c.b, c.out, 3) < 1) unported(what " on contact"); \
    return relay::undefined;                                                            \
  }
ADV_CONTACT_STATUS(static_onDamagingHit, "ability static")
ADV_CONTACT_STATUS(flamebody_onDamagingHit, "ability flamebody")
ADV_CONTACT_STATUS(poisonpoint_onDamagingHit, "ability poisonpoint")
ADV_CONTACT_STATUS(cutecharm_onDamagingHit, "ability cutecharm")
#undef ADV_CONTACT_STATUS
ADV_HANDLER(effectspore_onDamagingHit) {
  ADV_UNUSED;
  if (made_contact(value, source_effect) && random(c.b, c.out, 10) < 1) unported("ability effectspore on contact");
  return relay::undefined;
}
ADV_HANDLER(colorchange_onDamagingHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (!value.number || !p.hp) return relay::undefined;
  const Battle::ActiveMove& move = c.b.active_move;
  if (p.is_active && move.category != Category::Status && move.type != Type::Unknown && !has_type(p, move.type)) {
    unported("ability colorchange changing its type");
  }
  return relay::undefined;
}
ADV_HANDLER(serenegrace_onModifyMove) {
  ADV_UNUSED;
  c.b.active_move.secondary_chance = uint8_t(std::min(c.b.active_move.secondary_chance * 2, 255));
  return relay::undefined;
}
ADV_HANDLER(damp_onAnyTryMove) {
  ADV_UNUSED;
  const Move move = Move(source_effect.id);
  if (move == Move::Explosion || move == Move::SelfDestruct) unported("ability damp stopping an explosion");
  return relay::undefined;
}
ADV_HANDLER(owntempo_onHit) {
  ADV_UNUSED;
  if (source_effect.kind == EffectKind::Move && data(Move(source_effect.id)).volatile_status == Condition::Confusion) {
    unported("ability owntempo against a confusing move");
  }
  return relay::undefined;
}

// Items.
ADV_CHAIN(blackbelt_onModifyAtk, move_type(c, source_effect) == Type::Fighting, 1.1, 1)
ADV_CHAIN(hardstone_onModifyAtk, move_type(c, source_effect) == Type::Rock, 1.1, 1)
ADV_CHAIN(metalcoat_onModifyAtk, move_type(c, source_effect) == Type::Steel, 1.1, 1)
ADV_CHAIN(poisonbarb_onModifyAtk, move_type(c, source_effect) == Type::Poison, 1.1, 1)
ADV_CHAIN(sharpbeak_onModifyAtk, move_type(c, source_effect) == Type::Flying, 1.1, 1)
ADV_CHAIN(silkscarf_onModifyAtk, move_type(c, source_effect) == Type::Normal, 1.1, 1)
ADV_CHAIN(silverpowder_onModifyAtk, move_type(c, source_effect) == Type::Bug, 1.1, 1)
ADV_CHAIN(softsand_onModifyAtk, move_type(c, source_effect) == Type::Ground, 1.1, 1)
ADV_CHAIN(spelltag_onModifyAtk, move_type(c, source_effect) == Type::Ghost, 1.1, 1)
ADV_CHAIN(blackglasses_onModifySpA, move_type(c, source_effect) == Type::Dark, 1.1, 1)
ADV_CHAIN(charcoal_onModifySpA, move_type(c, source_effect) == Type::Fire, 1.1, 1)
ADV_CHAIN(dragonfang_onModifySpA, move_type(c, source_effect) == Type::Dragon, 1.1, 1)
ADV_CHAIN(magnet_onModifySpA, move_type(c, source_effect) == Type::Electric, 1.1, 1)
ADV_CHAIN(miracleseed_onModifySpA, move_type(c, source_effect) == Type::Grass, 1.1, 1)
ADV_CHAIN(mysticwater_onModifySpA, move_type(c, source_effect) == Type::Water, 1.1, 1)
ADV_CHAIN(nevermeltice_onModifySpA, move_type(c, source_effect) == Type::Ice, 1.1, 1)
ADV_CHAIN(twistedspoon_onModifySpA, move_type(c, source_effect) == Type::Psychic, 1.1, 1)
ADV_CHAIN(seaincense_onModifySpA, move_type(c, source_effect) == Type::Water, 1.05, 1)
ADV_CHAIN(choiceband_onModifyAtk, true, 1.5, 1)
ADV_CHAIN(lightball_onModifySpA, c.b.at(t.pokemon).species == Species::Pikachu, 2, 1)
ADV_CHAIN(deepseatooth_onModifySpA, c.b.at(t.pokemon).species == Species::Clamperl, 2, 1)
ADV_CHAIN(deepseascale_onModifySpD, c.b.at(t.pokemon).species == Species::Clamperl, 2, 1)
ADV_CHAIN(thickclub_onModifyAtk, c.b.at(t.pokemon).species == Species::Cubone || c.b.at(t.pokemon).species == Species::Marowak, 2, 1)
inline bool lati(const Pokemon& p) { return p.base_species == Species::Latias || p.base_species == Species::Latios; }
ADV_CHAIN(souldew_onModifySpA, lati(c.b.at(t.pokemon)), 1.5, 1)
ADV_CHAIN(souldew_onModifySpD, lati(c.b.at(t.pokemon)), 1.5, 1)
// Nothing transforms yet, so a Ditto is itself.
ADV_CHAIN(metalpowder_onModifyDef, c.b.at(t.pokemon).species == Species::Ditto, 2, 1)
// onModifyAccuracy(accuracy): a number is scaled, not chained.
ADV_HANDLER(brightpowder_onModifyAccuracy) {
  ADV_UNUSED;
  return value.kind == Relay::Kind::Number ? relay::number(value.number * 0.9) : relay::undefined;
}
ADV_HANDLER(laxincense_onModifyAccuracy) {
  ADV_UNUSED;
  return value.kind == Relay::Kind::Number ? relay::number(value.number * 0.95) : relay::undefined;
}
ADV_HANDLER(scopelens_onModifyCritRatio) {
  ADV_UNUSED;
  return relay::number(value.number + 1);
}
ADV_HANDLER(luckypunch_onModifyCritRatio) {
  ADV_UNUSED;
  return c.b.at(t.pokemon).species == Species::Chansey ? relay::number(value.number + 2) : relay::undefined;
}
ADV_HANDLER(stick_onModifyCritRatio) {
  ADV_UNUSED;
  return c.b.at(t.pokemon).species == Species::Farfetchd ? relay::number(value.number + 2) : relay::undefined;
}
ADV_HANDLER(choiceband_onAfterMove) {
  ADV_UNUSED;
  add_volatile(c, t.pokemon, Condition::Choicelock);
  return relay::undefined;
}
ADV_HANDLER(shellbell_onAfterMoveSecondarySelf) {
  ADV_UNUSED;
  const uint16_t total = c.b.active_move.total_damage;
  if (total && !c.b.at(t.pokemon).force_switch_flag) heal(c, double(total) / 8);
  return relay::undefined;
}

// The lock a Choice Band puts on its holder (data/conditions.ts).
ADV_HANDLER(choicelock_onStart) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (p.last_move == Move::None) return relay::no;
  c.effect_state->move = p.last_move;
  return relay::undefined;
}
ADV_HANDLER(choicelock_onDisableMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  const Move locked = c.effect_state->move;
  bool has_move = false;
  for (int i = 0; i < p.moves; i++) has_move |= p.move_slots[i].id == locked;
  if (!data(p.item).is_choice || !has_move) {
    remove_volatile(c, t.pokemon, Condition::Choicelock);
    return relay::undefined;
  }
  // disableMove: every other move, openly.
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].id != locked) p.move_slots[i].disabled = Disabled::Yes;
  }
  return relay::undefined;
}
#undef ADV_CHAIN

#undef ADV_HANDLER
#undef ADV_UNUSED
#undef ADV_RULE_BEGIN
#undef ADV_BERRY_ON_UPDATE

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
    A(LiquidOoze, onSourceTryHeal, liquidooze_onSourceTryHeal),
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
    A(Insomnia, onSetStatus, insomnia_onSetStatus),
    A(VitalSpirit, onSetStatus, vitalspirit_onSetStatus),
    A(Immunity, onSetStatus, immunity_onSetStatus),
    A(Limber, onSetStatus, limber_onSetStatus),
    A(WaterVeil, onSetStatus, waterveil_onSetStatus),
    A(Synchronize, onAfterSetStatus, synchronize_onAfterSetStatus),
    A(ShieldDust, onModifySecondaries, shielddust_onModifySecondaries),
    A(InnerFocus, onTryAddVolatile, innerfocus_onTryAddVolatile),
    A(OwnTempo, onTryAddVolatile, owntempo_onTryAddVolatile),
    A(Insomnia, onTryAddVolatile, insomnia_onTryAddVolatile),
    A(VitalSpirit, onTryAddVolatile, vitalspirit_onTryAddVolatile),
    A(Pressure, onDeductPP, pressure_onDeductPP),
    A(Hustle, onModifyAtk, hustle_onModifyAtk),
    A(Hustle, onSourceModifyAccuracy, hustle_onSourceModifyAccuracy),
    A(Minus, onModifySpA, minus_onModifySpA),
    A(Plus, onModifySpA, plus_onModifySpA),
    A(Blaze, onBasePower, blaze_onBasePower),
    A(Overgrow, onBasePower, overgrow_onBasePower),
    A(Swarm, onBasePower, swarm_onBasePower),
    A(Torrent, onBasePower, torrent_onBasePower),
    A(ThickFat, onSourceBasePower, thickfat_onSourceBasePower),
    A(CompoundEyes, onSourceModifyAccuracy, compoundeyes_onSourceModifyAccuracy),
    A(SandVeil, onModifyAccuracy, sandveil_onModifyAccuracy),
    A(Guts, onModifyAtk, guts_onModifyAtk),
    A(HugePower, onModifyAtk, hugepower_onModifyAtk),
    A(PurePower, onModifyAtk, purepower_onModifyAtk),
    A(MarvelScale, onModifyDef, marvelscale_onModifyDef),
    A(BattleArmor, onCriticalHit, battlearmor_onCriticalHit),
    A(ShellArmor, onCriticalHit, shellarmor_onCriticalHit),
    A(Sturdy, onTryHit, sturdy_onTryHit),
    A(VoltAbsorb, onTryHit, voltabsorb_onTryHit),
    A(WaterAbsorb, onTryHit, waterabsorb_onTryHit),
    A(Soundproof, onTryHit, soundproof_onTryHit),
    A(Oblivious, onTryHit, oblivious_onTryHit),
    A(WonderGuard, onTryHit, wonderguard_onTryHit),
    A(FlashFire, onTryHit, flashfire_onTryHit),
    A(FlashFire, onEnd, flashfire_onEnd),
    A(RoughSkin, onDamagingHit, roughskin_onDamagingHit),
    A(Static, onDamagingHit, static_onDamagingHit),
    A(FlameBody, onDamagingHit, flamebody_onDamagingHit),
    A(PoisonPoint, onDamagingHit, poisonpoint_onDamagingHit),
    A(CuteCharm, onDamagingHit, cutecharm_onDamagingHit),
    A(EffectSpore, onDamagingHit, effectspore_onDamagingHit),
    A(ColorChange, onDamagingHit, colorchange_onDamagingHit),
    A(SereneGrace, onModifyMove, serenegrace_onModifyMove),
    A(Damp, onAnyTryMove, damp_onAnyTryMove),
    A(OwnTempo, onHit, owntempo_onHit),
    I(BlackBelt, onModifyAtk, blackbelt_onModifyAtk),
    I(HardStone, onModifyAtk, hardstone_onModifyAtk),
    I(MetalCoat, onModifyAtk, metalcoat_onModifyAtk),
    I(PoisonBarb, onModifyAtk, poisonbarb_onModifyAtk),
    I(SharpBeak, onModifyAtk, sharpbeak_onModifyAtk),
    I(SilkScarf, onModifyAtk, silkscarf_onModifyAtk),
    I(SilverPowder, onModifyAtk, silverpowder_onModifyAtk),
    I(SoftSand, onModifyAtk, softsand_onModifyAtk),
    I(SpellTag, onModifyAtk, spelltag_onModifyAtk),
    I(BlackGlasses, onModifySpA, blackglasses_onModifySpA),
    I(Charcoal, onModifySpA, charcoal_onModifySpA),
    I(DragonFang, onModifySpA, dragonfang_onModifySpA),
    I(Magnet, onModifySpA, magnet_onModifySpA),
    I(MiracleSeed, onModifySpA, miracleseed_onModifySpA),
    I(MysticWater, onModifySpA, mysticwater_onModifySpA),
    I(NeverMeltIce, onModifySpA, nevermeltice_onModifySpA),
    I(TwistedSpoon, onModifySpA, twistedspoon_onModifySpA),
    I(SeaIncense, onModifySpA, seaincense_onModifySpA),
    I(ChoiceBand, onModifyAtk, choiceband_onModifyAtk),
    I(ChoiceBand, onAfterMove, choiceband_onAfterMove),
    I(LightBall, onModifySpA, lightball_onModifySpA),
    I(DeepSeaTooth, onModifySpA, deepseatooth_onModifySpA),
    I(DeepSeaScale, onModifySpD, deepseascale_onModifySpD),
    I(ThickClub, onModifyAtk, thickclub_onModifyAtk),
    I(SoulDew, onModifySpA, souldew_onModifySpA),
    I(SoulDew, onModifySpD, souldew_onModifySpD),
    I(MetalPowder, onModifyDef, metalpowder_onModifyDef),
    I(BrightPowder, onModifyAccuracy, brightpowder_onModifyAccuracy),
    I(LaxIncense, onModifyAccuracy, laxincense_onModifyAccuracy),
    I(ScopeLens, onModifyCritRatio, scopelens_onModifyCritRatio),
    I(LuckyPunch, onModifyCritRatio, luckypunch_onModifyCritRatio),
    I(Stick, onModifyCritRatio, stick_onModifyCritRatio),
    I(ShellBell, onAfterMoveSecondarySelf, shellbell_onAfterMoveSecondarySelf),
    C(FlashFire, onStart, flashfire_condition_onStart),
    C(FlashFire, onEnd, flashfire_condition_onEnd),
    C(FlashFire, onModifyDamagePhase1, flashfire_condition_onModifyDamagePhase1),
    C(Choicelock, onStart, choicelock_onStart),
    C(Choicelock, onDisableMove, choicelock_onDisableMove),
    C(Par, onModifySpe, par_onModifySpe),
    C(Par, onStart, par_onStart),
    C(Par, onBeforeMove, par_onBeforeMove),
    C(Brn, onStart, brn_onStart),
    C(Brn, onResidual, brn_onResidual),
    C(Psn, onStart, psn_onStart),
    C(Psn, onResidual, psn_onResidual),
    C(Tox, onStart, tox_onStart),
    C(Tox, onSwitchIn, tox_onSwitchIn),
    C(Tox, onResidual, tox_onResidual),
    C(Slp, onStart, slp_onStart),
    C(Slp, onSwitchOut, slp_onSwitchOut),
    C(Slp, onBeforeMove, slp_onBeforeMove),
    C(Frz, onStart, frz_onStart),
    C(Frz, onBeforeMove, frz_onBeforeMove),
    C(Frz, onModifyMove, frz_onModifyMove),
    C(Frz, onAfterMoveSecondary, frz_onAfterMoveSecondary),
    C(Frz, onDamagingHit, frz_onDamagingHit),
    C(Flinch, onBeforeMove, flinch_onBeforeMove),
    {effect(Move::Rest), Callback::onTry, &rest_onTry},
    {effect(Move::Rest), Callback::onHit, &rest_onHit},
    {effect(Move::Refresh), Callback::onHit, &refresh_onHit},
    {effect(Move::Thunder), Callback::onModifyMove, &thunder_onModifyMove},
    {effect(Move::Curse), Callback::onModifyMove, &curse_onModifyMove},
    {effect(Move::Curse), Callback::onTryHit, &curse_onTryHit},
    C(Sandstorm, onFieldEnd, sandstorm_onFieldEnd),
    C(Raindance, durationCallback, raindance_durationCallback),
    C(Raindance, onFieldStart, raindance_onFieldStart),
    C(Raindance, onFieldEnd, raindance_onFieldEnd),
    C(Raindance, onFieldResidual, raindance_onFieldResidual),
    C(Raindance, onWeatherModifyDamage, raindance_onWeatherModifyDamage),
    C(Sunnyday, durationCallback, sunnyday_durationCallback),
    C(Sunnyday, onFieldStart, sunnyday_onFieldStart),
    C(Sunnyday, onFieldEnd, sunnyday_onFieldEnd),
    C(Sunnyday, onFieldResidual, sunnyday_onFieldResidual),
    C(Sunnyday, onWeatherModifyDamage, sunnyday_onWeatherModifyDamage),
    C(Hail, durationCallback, hail_durationCallback),
    C(Hail, onFieldStart, hail_onFieldStart),
    C(Hail, onFieldEnd, hail_onFieldEnd),
    C(Hail, onFieldResidual, hail_onFieldResidual),
    C(Hail, onWeather, hail_onWeather),
    {effect(Move::HiddenPower), Callback::onModifyMove, &hiddenpower_onModifyMove},
    {effect(Move::HiddenPower), Callback::onModifyType, &hiddenpower_onModifyType},
    {effect(Move::HiddenPower), Callback::basePowerCallback, &hiddenpower_basePowerCallback},
    {effect(Move::Return), Callback::basePowerCallback, &return_basePowerCallback},
    {effect(Move::Frustration), Callback::basePowerCallback, &frustration_basePowerCallback},
    R(SleepClauseMod, onSetStatus, sleepclausemod_onSetStatus),
    R(FreezeClauseMod, onSetStatus, freezeclausemod_onSetStatus),
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
