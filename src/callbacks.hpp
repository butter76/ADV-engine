// Showdown's handlers, ported one for one: a function here is the callback
// of the same effect and name in Showdown's data (the checklist of
// tools/dump_dex.cjs). A branch that needs something the engine does not
// have yet calls unported() and says what.

#pragma once

#include "moves.hpp"

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
ADV_HANDLER(drizzle_onStart) {
  ADV_UNUSED;
  set_weather(c, Condition::Raindance);
  return relay::undefined;
}
ADV_HANDLER(drought_onStart) {
  ADV_UNUSED;
  set_weather(c, Condition::Sunnyday);
  return relay::undefined;
}
// Forecast: Castform takes the weather's form.
ADV_HANDLER(forecast_onWeatherChange) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  const Species base = p.base_species;
  if ((base != Species::Castform && base != Species::CastformSunny && base != Species::CastformRainy &&
       base != Species::CastformSnowy) || p.transformed) {
    return relay::undefined;
  }
  Species forme = Species::Castform;
  switch (effective_weather(c.b)) {
  case Condition::Sunnyday:
  case Condition::Desolateland: forme = Species::CastformSunny; break;
  case Condition::Raindance:
  case Condition::Primordialsea: forme = Species::CastformRainy; break;
  case Condition::Hail:
  case Condition::Snowscape: forme = Species::CastformSnowy; break;
  default: break;
  }
  if (p.is_active && p.species != forme) forme_change(c, t.pokemon, forme, Ability::Forecast);
  return relay::undefined;
}
ADV_HANDLER(forecast_onStart) {
  ADV_UNUSED;
  single_event(c, callback("", "WeatherChange"), effect(Ability::Forecast), c.effect_state, t);
  return relay::undefined;
}
ADV_HANDLER(raindish_onResidual) {
  ADV_UNUSED;
  const Condition weather = effective_weather(c.b);
  if (weather == Condition::Raindance || weather == Condition::Primordialsea) heal(c, double(c.b.at(t.pokemon).base_maxhp) / 16);
  return relay::undefined;
}
// Damp's onAnyDamage is for Aftermath, which Gen 3 does not have.
ADV_HANDLER(damp_onAnyDamage) {
  ADV_UNUSED;
  return relay::undefined;
}
// White Herb: used when a stat is lowered, looked for at several times.
ADV_HANDLER(whiteherb_onStart) {
  ADV_UNUSED;
  const Ref holder = c.effect_state->target_pokemon;
  const Boosts& boosts = c.b.at(holder).boosts;
  const int8_t* v = &boosts.atk;
  bool ready = false;
  for (int i = 0; i < BOOST_STATS; i++) ready |= v[i] < 0;
  if (ready) use_item(c, holder);
  return relay::undefined;
}
ADV_HANDLER(whiteherb_onUse) {
  ADV_UNUSED;
  int8_t* v = &c.b.at(t.pokemon).boosts.atk;
  for (int i = 0; i < BOOST_STATS; i++) v[i] = std::max<int8_t>(v[i], 0);
  add(c, {part::text("-clearnegativeboost"), part::pokemon(c.b, t.pokemon), part::text("[silent]")});
  return relay::undefined;
}
// King's Rock gives most damaging moves a chance to flinch, besides what
// they have.
ADV_HANDLER(kingsrock_onModifyMove) {
  ADV_UNUSED;
  static constexpr Move AFFECTED[] = {
      Move::AerialAce, Move::Aeroblast, Move::AirCutter, Move::ArmThrust, Move::Barrage, Move::BeatUp, Move::Bide, Move::Bind,
      Move::BlastBurn, Move::BoneRush, Move::Bonemerang, Move::Bounce, Move::BrickBreak, Move::BulletSeed, Move::Clamp,
      Move::CometPunch, Move::Crabhammer, Move::CrossChop, Move::Cut, Move::Dig, Move::Dive, Move::DoubleKick, Move::DoubleSlap,
      Move::DoubleEdge, Move::DragonBreath, Move::DragonClaw, Move::DragonRage, Move::DrillPeck, Move::Earthquake, Move::EggBomb,
      Move::Endeavor, Move::Eruption, Move::Explosion, Move::ExtremeSpeed, Move::FalseSwipe, Move::FeintAttack, Move::FireSpin,
      Move::Flail, Move::Fly, Move::FrenzyPlant, Move::Frustration, Move::FuryAttack, Move::FuryCutter, Move::FurySwipes,
      Move::Gust, Move::HiddenPower, Move::HighJumpKick, Move::HornAttack, Move::HydroCannon, Move::HydroPump, Move::HyperBeam,
      Move::IceBall, Move::IcicleSpear, Move::JumpKick, Move::KarateChop, Move::LeafBlade, Move::LowKick, Move::MachPunch,
      Move::MagicalLeaf, Move::Magnitude, Move::MegaKick, Move::MegaPunch, Move::Megahorn, Move::MeteorMash, Move::MudShot,
      Move::MuddyWater, Move::NightShade, Move::Outrage, Move::Overheat, Move::PayDay, Move::Peck, Move::PetalDance,
      Move::PinMissile, Move::PoisonTail, Move::Pound, Move::PsychoBoost, Move::Psywave, Move::QuickAttack, Move::Rage,
      Move::RapidSpin, Move::RazorLeaf, Move::RazorWind, Move::Return, Move::Revenge, Move::Reversal, Move::RockBlast,
      Move::RockThrow, Move::RollingKick, Move::Rollout, Move::SandTomb, Move::Scratch, Move::SeismicToss, Move::SelfDestruct,
      Move::ShadowPunch, Move::ShockWave, Move::SignalBeam, Move::SilverWind, Move::SkullBash, Move::SkyAttack,
      Move::SkyUppercut, Move::Slam, Move::Slash, Move::Snore, Move::SolarBeam, Move::SonicBoom, Move::SpikeCannon, Move::SpitUp,
      Move::SteelWing, Move::Strength, Move::Struggle, Move::Submission, Move::Surf, Move::Swift, Move::Tackle, Move::TakeDown,
      Move::Thrash, Move::TripleKick, Move::Twister, Move::Uproar, Move::ViseGrip, Move::VineWhip, Move::VitalThrow,
      Move::VoltTackle, Move::WaterGun, Move::WaterPulse, Move::Waterfall, Move::WeatherBall, Move::Whirlpool, Move::WingAttack,
      Move::Wrap};
  // A typed Hidden Power is Hidden Power.
  Move id = c.b.active_move.id;
  if (data(id).real_move != Move::None) id = data(id).real_move;
  for (const Move m : AFFECTED) {
    if (m == id) c.b.active_move.flinch_chance = 10;
  }
  return relay::undefined;
}
ADV_HANDLER(lansatberry_onEat) {
  ADV_UNUSED;
  add_volatile(c, t.pokemon, Condition::FocusEnergy);
  return relay::undefined;
}
// Mail can only be knocked off or stolen.
ADV_HANDLER(mail_onTakeItem) {
  ADV_UNUSED;
  if (!c.b.has_active_move) return relay::no;
  const Move id = c.b.active_move.id;
  return id != Move::KnockOff && id != Move::Thief && id != Move::Covet ? relay::no : relay::undefined;
}
ADV_HANDLER(focusenergy_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Focus Energy")});
  return relay::undefined;
}
ADV_HANDLER(focusenergy_condition_onModifyCritRatio) {
  ADV_UNUSED;
  return relay::number(value.number + 2);
}
// Charge doubles the next Electric move.
ADV_HANDLER(charge_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Charge")});
  return relay::undefined;
}
ADV_HANDLER(charge_condition_onBasePower) {
  ADV_UNUSED;
  if (c.b.active_move.type == Type::Electric) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(charge_condition_onAfterMove) {
  ADV_UNUSED;
  if (Move(source_effect.id) != Move::Charge) remove_volatile(c, t.pokemon, Condition::Charge);
  return relay::undefined;
}
ADV_HANDLER(charge_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Charge"), part::text("[silent]")});
  return relay::undefined;
}
// Wrap and its kind: held in, and hurt each turn while the user stays.
ADV_HANDLER(partiallytrapped_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::fullname(c.effect_state->source_effect), part::of(c.b, source)});
  return relay::undefined;
}
ADV_HANDLER(partiallytrapped_onResidual) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  const Ref trapper = c.effect_state->source;
  if (trapper && (!c.b.at(trapper).is_active || !c.b.at(trapper).hp || !c.b.at(trapper).active_turns)) {
    // Deleted: nothing is run for its end.
    const Effect by = c.effect_state->source_effect;
    if (Volatile* v = volatile_of(p, Condition::Partiallytrapped)) v->id = Condition::None;
    add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::name(by), part::text("[partiallytrapped]"), part::text("[silent]")});
    return relay::undefined;
  }
  damage(c, double(p.base_maxhp) / 16);
  return relay::undefined;
}
ADV_HANDLER(partiallytrapped_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::name(c.effect_state->source_effect), part::text("[partiallytrapped]")});
  return relay::undefined;
}
ADV_HANDLER(partiallytrapped_onTrapPokemon) {
  ADV_UNUSED;
  const Ref trapper = c.effect_state->source;
  if (trapper && c.b.at(trapper).is_active) try_trap(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(intimidate_onStart) {
  ADV_UNUSED;
  // adjacentFoes(): in singles the one foe, if it has HP.
  const Ref foe = foe_of(c.b, t.pokemon);
  const bool behind_substitute = foe && volatile_of(c.b.at(foe), Condition::Substitute);
  if (!foe || behind_substitute) {
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
  const bool dream_eater = c.b.has_active_move && c.b.active_move.id == Move::DreamEater;
  if ((source_effect == effect(Special::Drain) || source_effect == effect(Condition::LeechSeed)) && !dream_eater) {
    // The one that would have healed is hurt instead, and faints at once if
    // that takes its last HP: its faint goes before any already waiting.
    const double dealt = damage(c, value.number, t.pokemon, source);
    if (dealt && !c.b.at(t.pokemon).hp) {
      Battle& b = c.b;
      if (b.faints > 1) {
        const Battle::Faint last = b.faint_queue[size_t(b.faints - 1)];
        for (int i = b.faints - 1; i > 0; i--) b.faint_queue[size_t(i)] = b.faint_queue[size_t(i - 1)];
        b.faint_queue[0] = last;
      }
      resolve_faints(c);
    }
    return relay::number(0);
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
// Truant loafs every other turn, marked by a volatile of its name.
inline void truant_turn(Ctx& c, Ref r, bool loafs) {
  c.b.at(r).truant_turn = loafs;
  if (loafs) add_volatile(c, r, Condition::Truant);
  else remove_volatile(c, r, Condition::Truant);
}
ADV_HANDLER(truant_onSwitchIn) {
  ADV_UNUSED;
  truant_turn(c, t.pokemon, c.b.turn != 0);
  return relay::undefined;
}
ADV_HANDLER(truant_onResidual) {
  ADV_UNUSED;
  truant_turn(c, t.pokemon, !c.b.at(t.pokemon).truant_turn);
  return relay::undefined;
}
ADV_HANDLER(truant_onBeforeMove) {
  ADV_UNUSED;
  if (!volatile_of(c.b.at(t.pokemon), Condition::Truant)) return relay::undefined;
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("ability: Truant")});
  return relay::no;
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
  if (volatile_of(c.b.at(t.pokemon), Condition::Attract)) {
    remove_volatile(c, t.pokemon, Condition::Attract);
    add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("move: Attract"), part::from(effect(Ability::Oblivious))});
  }
  return relay::undefined;
}
ADV_HANDLER(owntempo_onUpdate) {
  ADV_UNUSED;
  if (volatile_of(c.b.at(t.pokemon), Condition::Confusion)) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("ability: Own Tempo")});
    remove_volatile(c, t.pokemon, Condition::Confusion);
  }
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
  if (value.text != "yawn") return relay::undefined;
  add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::Insomnia))});
  return relay::null;
}
ADV_HANDLER(vitalspirit_onTryAddVolatile) {
  ADV_UNUSED;
  if (value.text != "yawn") return relay::undefined;
  add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::VitalSpirit))});
  return relay::null;
}

// ---------------------------------------------------------------------------
// Items (data/items.ts, data/mods/gen3/items.ts). The berries eat themselves
// on the status they cure.
// ---------------------------------------------------------------------------
// The berries eaten as soon as their Pokémon has the status they cure.
#define ADV_BERRY_ON_UPDATE(name, cures, what)                          \
  ADV_HANDLER(name) {                                                   \
    ADV_UNUSED;                                                         \
    (void)what;                                                         \
    const Condition status = c.b.at(t.pokemon).status;                  \
    if (cures) eat_item(c, t.pokemon);                                  \
    return relay::undefined;                                            \
  }
ADV_BERRY_ON_UPDATE(aspearberry_onUpdate, status == Condition::Frz, "item aspearberry")
ADV_BERRY_ON_UPDATE(cheriberry_onUpdate, status == Condition::Par, "item cheriberry")
ADV_BERRY_ON_UPDATE(chestoberry_onUpdate, status == Condition::Slp, "item chestoberry")
ADV_BERRY_ON_UPDATE(lumberry_onUpdate, status != Condition::None || volatile_of(c.b.at(t.pokemon), Condition::Confusion), "item lumberry")
ADV_BERRY_ON_UPDATE(pechaberry_onUpdate, status == Condition::Psn || status == Condition::Tox, "item pechaberry")
ADV_BERRY_ON_UPDATE(rawstberry_onUpdate, status == Condition::Brn, "item rawstberry")
ADV_BERRY_ON_UPDATE(persimberry_onUpdate, (void(status), volatile_of(c.b.at(t.pokemon), Condition::Confusion) != nullptr), "item persimberry")
// Mental Herb is used up to end an attraction.
ADV_HANDLER(mentalherb_onUpdate) {
  ADV_UNUSED;
  if (volatile_of(c.b.at(t.pokemon), Condition::Attract) && use_item(c, t.pokemon)) {
    remove_volatile(c, t.pokemon, Condition::Attract);
    add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("move: Attract"), part::from(effect(Item::MentalHerb))});
  }
  return relay::undefined;
}
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
  // The hit a confused Pokémon gives itself is a move's too.
  const bool by_move = (source_effect.kind == EffectKind::Move && source_effect) || source_effect == effect(Special::Confused);
  if (lucky && value.number >= c.b.at(t.pokemon).hp && by_move) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("item: Focus Band")});
    return relay::number(c.b.at(t.pokemon).hp - 1);
  }
  return relay::undefined;
}
// The berries eaten at the end of a turn at half or a quarter of full HP.
#define ADV_PINCH_BERRY(name, divisor, what)                                        \
  ADV_HANDLER(name) {                                                               \
    ADV_UNUSED;                                                                     \
    const Pokemon& p = c.b.at(t.pokemon);                                           \
    (void)what;                                                                     \
    if (double(p.hp) <= double(p.maxhp) / divisor) eat_item(c, t.pokemon);          \
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
// What eating a berry does (onEat), and for the healing ones whether it can
// be eaten (onTryEatItem asks TryHeal, which nothing in Gen 3 refuses a
// berry).
#define ADV_TRY_EAT(name)                                              \
  ADV_HANDLER(name) {                                                  \
    ADV_UNUSED;                                                        \
    return run_event(c, TRY_HEAL, on(t.pokemon), {}, c.effect, relay::number(1)).truthy() ? relay::undefined : relay::no; \
  }
ADV_TRY_EAT(aguavberry_onTryEatItem)
ADV_TRY_EAT(figyberry_onTryEatItem)
ADV_TRY_EAT(iapapaberry_onTryEatItem)
ADV_TRY_EAT(magoberry_onTryEatItem)
ADV_TRY_EAT(wikiberry_onTryEatItem)
ADV_TRY_EAT(oranberry_onTryEatItem)
ADV_TRY_EAT(sitrusberry_onTryEatItem)
#undef ADV_TRY_EAT
ADV_HANDLER(aguavberry_onEat) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  const NatureData& nature = data(c.b.at(t.pokemon).set.nature);
  if (nature.plus != nature.minus && nature.minus == Stat::Spd) add_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(figyberry_onEat) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  const NatureData& nature = data(c.b.at(t.pokemon).set.nature);
  if (nature.plus != nature.minus && nature.minus == Stat::Atk) add_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(iapapaberry_onEat) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  const NatureData& nature = data(c.b.at(t.pokemon).set.nature);
  if (nature.plus != nature.minus && nature.minus == Stat::Def) add_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(magoberry_onEat) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  const NatureData& nature = data(c.b.at(t.pokemon).set.nature);
  if (nature.plus != nature.minus && nature.minus == Stat::Spe) add_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(wikiberry_onEat) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 8);
  const NatureData& nature = data(c.b.at(t.pokemon).set.nature);
  if (nature.plus != nature.minus && nature.minus == Stat::Spa) add_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(oranberry_onEat) {
  ADV_UNUSED;
  heal(c, 10);
  return relay::undefined;
}
ADV_HANDLER(sitrusberry_onEat) {
  ADV_UNUSED;
  heal(c, 30);
  return relay::undefined;
}
ADV_HANDLER(apicotberry_onEat) {
  ADV_UNUSED;
  SparseBoosts up;
  up.set(3, 1);
  boost(c, up, t.pokemon, c.event.source);
  return relay::undefined;
}
ADV_HANDLER(ganlonberry_onEat) {
  ADV_UNUSED;
  SparseBoosts up;
  up.set(1, 1);
  boost(c, up, t.pokemon, c.event.source);
  return relay::undefined;
}
ADV_HANDLER(liechiberry_onEat) {
  ADV_UNUSED;
  SparseBoosts up;
  up.set(0, 1);
  boost(c, up, t.pokemon, c.event.source);
  return relay::undefined;
}
ADV_HANDLER(petayaberry_onEat) {
  ADV_UNUSED;
  SparseBoosts up;
  up.set(2, 1);
  boost(c, up, t.pokemon, c.event.source);
  return relay::undefined;
}
ADV_HANDLER(salacberry_onEat) {
  ADV_UNUSED;
  SparseBoosts up;
  up.set(4, 1);
  boost(c, up, t.pokemon, c.event.source);
  return relay::undefined;
}
ADV_HANDLER(starfberry_onEat) {
  ADV_UNUSED;
  // One of the five stats that can still rise, drawn, by two stages.
  const int8_t* stage = &c.b.at(t.pokemon).boosts.atk;
  std::array<int, 5> stats{};
  uint32_t n = 0;
  for (int stat = 0; stat < 5; stat++) {
    if (stage[stat] < 6) stats[n++] = stat;
  }
  if (n) {
    SparseBoosts up;
    up.set(stats[random(c.b, c.out, n)], 2);
    boost(c, up, t.pokemon, c.event.source);
  }
  return relay::undefined;
}
ADV_HANDLER(aspearberry_onEat) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status == Condition::Frz) cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(cheriberry_onEat) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status == Condition::Par) cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(chestoberry_onEat) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status == Condition::Slp) cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(pechaberry_onEat) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status == Condition::Psn || status == Condition::Tox) cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(rawstberry_onEat) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status == Condition::Brn) cure_status(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(lumberry_onEat) {
  ADV_UNUSED;
  cure_status(c, t.pokemon);
  remove_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(lumberry_onAfterSetStatus) {
  ADV_UNUSED;
  eat_item(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(persimberry_onEat) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(leppaberry_onEat) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  MoveSlot* slot = nullptr;
  for (int i = 0; !slot && i < p.moves; i++) {
    if (p.move_slots[i].pp == 0) slot = &p.move_slots[i];
  }
  for (int i = 0; !slot && i < p.moves; i++) {
    if (p.move_slots[i].pp < p.move_slots[i].maxpp) slot = &p.move_slots[i];
  }
  if (!slot) return relay::undefined;
  slot->pp = uint8_t(std::min(slot->pp + 10, int(slot->maxpp)));
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("item: Leppa Berry"), slot_name(p, *slot),
          part::text("[consumed]")});
  return relay::undefined;
}

// Confusion (data/conditions.ts).
ADV_HANDLER(confusion_onStart) {
  ADV_UNUSED;
  if (source_effect == effect(Condition::Lockedmove)) {
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("confusion"), part::text("[fatigue]")});
  } else if (source_effect.kind == EffectKind::Ability && source_effect) {
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("confusion"), part::from(source_effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("confusion")});
  }
  c.effect_state->time = int8_t(random(c.b, c.out, 2, 6));
  return relay::undefined;
}
ADV_HANDLER(confusion_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("confusion")});
  return relay::undefined;
}
ADV_HANDLER(confusion_onBeforeMove) {
  ADV_UNUSED;
  Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::Confusion);
  v->state.time--;
  if (!v->state.time) {
    remove_volatile(c, t.pokemon, Condition::Confusion);
    return relay::undefined;
  }
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("confusion")});
  if (random(c.b, c.out, 2) < 1) return relay::undefined; // randomChance(1, 2): it moves
  confusion_damage(c, t.pokemon);
  return relay::no;
}
ADV_HANDLER(leppaberry_onUpdate) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (!p.hp) return relay::undefined;
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].pp == 0) {
      eat_item(c, t.pokemon);
      break;
    }
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
  if (remove_volatile(c, t.pokemon, Condition::Nightmare)) {
    add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Nightmare"), part::text("[silent]")});
  }
  return relay::undefined;
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
  if (const Relay slept = set_status_result(c, t.pokemon, Condition::Slp, source, effect(Move::Rest)); !slept.truthy()) return slept;
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
// Focus Punch tightens its focus before the turn's moves and loses it when
// its user is hit by anything but a status move.
ADV_HANDLER(focuspunch_beforeTurnCallback) {
  ADV_UNUSED;
  add_volatile(c, t.pokemon, Condition::FocusPunch, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(focuspunch_onTry) {
  ADV_UNUSED;
  const Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::FocusPunch);
  if (v && v->state.lost_focus) {
    attr_last_move(c, "[still]");
    add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("Focus Punch"), part::text("Focus Punch")});
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(focuspunch_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singleturn"), part::pokemon(c.b, t.pokemon), part::text("move: Focus Punch")});
  return relay::undefined;
}
ADV_HANDLER(focuspunch_condition_onHit) {
  ADV_UNUSED;
  if (c.b.active_move.category != Category::Status) c.effect_state->lost_focus = true;
  return relay::undefined;
}
ADV_HANDLER(focuspunch_condition_onTryAddVolatile) {
  ADV_UNUSED;
  return value.text == "flinch" ? relay::null : relay::undefined;
}
ADV_HANDLER(suctioncups_onDragOut) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("ability: Suction Cups")});
  return relay::null;
}

// Leech Seed.
ADV_HANDLER(leechseed_onTryImmunity) {
  ADV_UNUSED;
  return has_type(c.b.at(t.pokemon), Type::Grass) ? relay::no : relay::yes;
}
ADV_HANDLER(leechseed_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Leech Seed")});
  return relay::undefined;
}
ADV_HANDLER(leechseed_condition_onResidual) {
  ADV_UNUSED;
  // Whatever stands where the seeder stood takes what the seed drains.
  const Side& other = c.b.sides[c.effect_state->source.side];
  if (other.active == 0xFF) return relay::undefined;
  const Ref leecher{c.effect_state->source.side, other.active};
  if (c.b.at(leecher).fainted || !c.b.at(leecher).hp) return relay::undefined;
  const double drained = damage(c, double(c.b.at(t.pokemon).base_maxhp) / 8, t.pokemon, leecher);
  if (drained) heal(c, drained, leecher, t.pokemon, c.effect);
  return relay::undefined;
}
// Taunt.
ADV_HANDLER(taunt_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Taunt")});
  return relay::undefined;
}
ADV_HANDLER(taunt_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("move: Taunt"), part::text("[silent]")});
  return relay::undefined;
}
ADV_HANDLER(taunt_condition_onDisableMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  for (int i = 0; i < p.moves; i++) {
    if (data(p.move_slots[i].id).category == Category::Status) p.move_slots[i].disabled = Disabled::Yes;
  }
  return relay::undefined;
}
ADV_HANDLER(taunt_condition_onBeforeMove) {
  ADV_UNUSED;
  if (c.b.active_move.category != Category::Status) return relay::undefined;
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("move: Taunt"), part::move(c.b.active_move.id)});
  return relay::no;
}
// Wish heals whatever stands in its slot when it ends.
ADV_HANDLER(wish_condition_onEnd) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).fainted) return relay::undefined;
  if (heal(c, double(c.b.at(t.pokemon).base_maxhp) / 2, t.pokemon, t.pokemon, c.effect)) {
    add(c, {part::text("-heal"), part::pokemon(c.b, t.pokemon), part::health(c.b, t.pokemon), part::from(effect(Move::Wish)),
            part::wisher(c.effect_state->source)});
  }
  return relay::undefined;
}
ADV_HANDLER(knockoff_onAfterHit) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.item == Item::None) return relay::undefined;
  const Item item = p.item;
  if (run_event(c, event("TakeItem"), on(t.pokemon), source, source_effect, relay::yes).truthy()) {
    p.item = Item::None;
    p.pending_staleness = Staleness::None;
    p.item_knocked_off = true;
    add(c, {part::text("-enditem"), part::pokemon(c.b, t.pokemon), part::item(item), part::from(effect(Move::KnockOff)),
            part::of(c.b, source)});
  }
  return relay::undefined;
}
// The moves that cure the user's team. `t` is the user.
ADV_HANDLER(aromatherapy_onHit) {
  ADV_UNUSED;
  add(c, {part::text("-cureteam"), part::pokemon(c.b, source), part::from(effect(Move::Aromatherapy))});
  const Side& side = c.b.sides[t.pokemon.side];
  for (int position = 0; position < side.size; position++) clear_status(c, {t.pokemon.side, index_at(side, position)});
  return relay::undefined;
}
ADV_HANDLER(healbell_onHit) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, source), part::text("move: Heal Bell")});
  const Side& side = c.b.sides[t.pokemon.side];
  for (int position = 0; position < side.size; position++) {
    const Ref ally{t.pokemon.side, index_at(side, position)};
    if (c.b.at(ally).ability == Ability::Soundproof) {
      if (c.b.at(ally).is_active) add(c, {part::text("-immune"), part::pokemon(c.b, ally), part::from(effect(Ability::Soundproof))});
      continue;
    }
    cure_status(c, ally, true);
  }
  return relay::undefined;
}
ADV_HANDLER(haze_onHitField) {
  ADV_UNUSED;
  add(c, {part::text("-clearallboost")});
  std::array<Ref, 2> actives;
  const int n = all_active(c.b, actives);
  for (int i = 0; i < n; i++) c.b.at(actives[i]).boosts = Boosts{};
  return relay::undefined;
}

// Pursuit catches a foe as it switches out: the chaser gives up its queued
// move and uses Pursuit then, at double power and without a miss.
ADV_HANDLER(pursuit_beforeTurnCallback) {
  ADV_UNUSED;
  add_volatile(c, t.pokemon, Condition::Pursuit, t.pokemon, effect(Move::Pursuit));
  return relay::undefined;
}
ADV_HANDLER(pursuit_basePowerCallback) {
  ADV_UNUSED;
  const Pokemon& target = c.b.at(source);
  return relay::number(target.being_called_back || target.switch_flag ? 80 : 40);
}
ADV_HANDLER(pursuit_onModifyMove) {
  ADV_UNUSED;
  const Pokemon& target = c.b.at(source);
  if (target.being_called_back || target.switch_flag) c.b.active_move.accuracy = 0;
  return relay::undefined;
}
ADV_HANDLER(pursuit_condition_onFoeBeforeSwitchOut) {
  ADV_UNUSED;
  Battle& b = c.b;
  const Ref chaser = c.effect_state->source;
  Pokemon& user = b.at(chaser);
  if (user.status == Condition::Frz || user.status == Condition::Slp) return relay::undefined;
  if (user.ability == Ability::Truant && volatile_of(user, Condition::Truant)) return relay::undefined;
  if (!is_adjacent(b, chaser, t.pokemon) || !user.hp) return relay::undefined;
  // Held to another move by Encore, it does not give chase.
  if (const Volatile* encore = volatile_of(user, Condition::Encore); encore && encore->state.move != Move::Pursuit) return relay::undefined;
  // queue.cancelMove: its queued move, if it still has one.
  bool cancelled = false;
  for (int i = 0; i < b.queue.size && !cancelled; i++) {
    if (b.queue.list[i].choice != Choice::Move || b.queue.list[i].pokemon != chaser) continue;
    for (int k = i + 1; k < b.queue.size; k++) b.queue.list[k - 1] = b.queue.list[k];
    b.queue.size--;
    cancelled = true;
  }
  if (!cancelled) return relay::undefined;
  Pokemon& leaving = b.at(t.pokemon);
  if (!leaving.pursuit_activated) {
    add(c, {part::text("-activate"), part::pokemon(b, t.pokemon), part::text("move: Pursuit")});
    leaving.pursuit_activated = true;
  }
  deduct_pp(user, Move::Pursuit);
  user.last_move = user.move_this_turn = Move::Pursuit;
  if (use_move(c, Move::Pursuit, chaser, t.pokemon, effect(Move::Pursuit)) && data(b.at(chaser).item).is_choice) {
    add_volatile(c, chaser, Condition::Choicelock, chaser);
  }
  return relay::undefined;
}
// Counter and Mirror Coat answer the last hit of their kind with twice its
// damage. Counter takes every Hidden Power for a physical move.
#define ADV_COUNTER(id, condition_, answers)                                                         \
  ADV_HANDLER(id##_beforeTurnCallback) {                                                             \
    ADV_UNUSED;                                                                                      \
    add_volatile(c, t.pokemon, Condition::condition_, t.pokemon);                                    \
    return relay::undefined;                                                                         \
  }                                                                                                  \
  ADV_HANDLER(id##_onTry) {                                                                          \
    ADV_UNUSED;                                                                                      \
    const Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::condition_);                       \
    return v && v->state.has_slot ? relay::undefined : relay::no;                                    \
  }                                                                                                  \
  ADV_HANDLER(id##_damageCallback) {                                                                 \
    ADV_UNUSED;                                                                                      \
    const Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::condition_);                       \
    if (!v) return relay::number(0);                                                                 \
    return relay::number(v->state.damage ? v->state.damage : 1);                                     \
  }                                                                                                  \
  ADV_HANDLER(id##_condition_onStart) {                                                              \
    ADV_UNUSED;                                                                                      \
    c.effect_state->has_slot = false;                                                                \
    c.effect_state->damage = 0;                                                                      \
    return relay::undefined;                                                                         \
  }                                                                                                  \
  ADV_HANDLER(id##_condition_onDamage) {                                                             \
    ADV_UNUSED;                                                                                      \
    const Battle::ActiveMove& move = c.b.active_move;                                                \
    if (source_effect.kind == EffectKind::Move && source_effect && source && source.side != t.pokemon.side && (answers)) { \
      c.effect_state->has_slot = true;                                                               \
      c.effect_state->damage = uint16_t(2 * value.number);                                           \
    }                                                                                                \
    return relay::undefined;                                                                         \
  }
ADV_COUNTER(counter, Counter, move.category == Category::Physical || move.id == Move::HiddenPower)
ADV_COUNTER(mirrorcoat, MirrorCoat, move.category == Category::Special && move.id != Move::HiddenPower)
#undef ADV_COUNTER

// Endeavor brings the target's HP down to its user's.
ADV_HANDLER(endeavor_onTry) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).hp >= c.b.at(source).hp) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon)});
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(endeavor_onTryImmunity) {
  ADV_UNUSED;
  return c.b.at(source).hp < c.b.at(t.pokemon).hp ? relay::yes : relay::no;
}
ADV_HANDLER(endeavor_damageCallback) {
  ADV_UNUSED;
  return relay::number(double(c.b.at(source).hp) - double(c.b.at(t.pokemon).hp));
}
// Perish Song: each active Pokémon it reaches is counted down from 3.
ADV_HANDLER(perishsong_onHitField) {
  ADV_UNUSED;
  bool result = false, message = false;
  std::array<Ref, 2> actives;
  const int n = all_active(c.b, actives);
  const Effect me = effect(Move::PerishSong);
  for (int i = 0; i < n; i++) {
    const Ref r = actives[i];
    if (run_event(c, events::INVULNERABILITY, on(r), source, me) == relay::no) {
      add(c, {part::text("-miss"), part::pokemon(c.b, source), part::pokemon(c.b, r)});
      result = true;
    } else if (run_event(c, events::TRY_HIT, on(r), source, me).kind == Relay::Kind::Null) {
      result = true;
    } else if (!volatile_of(c.b.at(r), Condition::PerishSong)) {
      add_volatile(c, r, Condition::PerishSong);
      add(c, {part::text("-start"), part::pokemon(c.b, r), part::text("perish3"), part::text("[silent]")});
      result = true;
      message = true;
    }
  }
  if (!result) return relay::no;
  if (message) add(c, {part::text("-fieldactivate"), part::text("move: Perish Song")});
  return relay::undefined;
}
ADV_HANDLER(perishsong_condition_onResidual) {
  ADV_UNUSED;
  static constexpr std::string_view COUNT[] = {"perish0", "perish1", "perish2", "perish3", "perish4"};
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text(COUNT[std::min<int>(c.effect_state->duration, 4)])});
  return relay::undefined;
}
ADV_HANDLER(perishsong_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("perish0")});
  faint(c, t.pokemon, {}, {});
  return relay::undefined;
}
// Thief and Covet take the target's item for a user that holds none.
inline void steal_item(Ctx& c, Ref target, Ref source, Move move) {
  Battle& b = c.b;
  if (b.at(source).item != Item::None) return;
  Pokemon& from = b.at(target);
  // Pokemon.takeItem(source).
  if (from.item == Item::None) return;
  const Item item = from.item;
  if (!run_event(c, event("TakeItem"), on(target), source, {}, relay::yes).truthy()) return;
  from.item = Item::None;
  const EffectState old_state = from.item_state;
  from.item_state = EffectState{};
  EffectState ended = old_state;
  single_event(c, callback("", "End"), effect(item), &ended, on(target));
  // The item's own onTakeItem, then Pokemon.setItem on the user.
  Pokemon& to = b.at(source);
  EffectState none{};
  if (!single_event(c, callback("", "TakeItem"), effect(item), &none, on(source), target, effect(move), relay::yes).truthy() || !to.hp ||
      !to.is_active) {
    from.item = item;
    refresh_mask(c.b, from);
    return;
  }
  from.pending_staleness = Staleness::None;
  to.pending_staleness = Staleness::Internal;
  to.item = item;
  refresh_mask(c.b, to);
  to.item_state = init_effect_state(b, true, source);
  single_event(c, ON_START, effect(item), &to.item_state, on(source), source, effect(move));
  if (move == Move::Thief) {
    add(c, {part::text("-enditem"), part::pokemon(b, target), part::item(item), part::text("[silent]"), part::from(effect(move)),
            part::of(b, source)});
  }
  add(c, {part::text("-item"), part::pokemon(b, source), part::item(item), part::from(effect(move)), part::of(b, target)});
}
ADV_HANDLER(thief_onAfterHit) {
  ADV_UNUSED;
  steal_item(c, t.pokemon, source, Move::Thief);
  return relay::undefined;
}
ADV_HANDLER(covet_onAfterHit) {
  ADV_UNUSED;
  steal_item(c, t.pokemon, source, Move::Covet);
  return relay::undefined;
}
ADV_HANDLER(stickyhold_onTakeItem) {
  ADV_UNUSED;
  if ((source && source != t.pokemon) || c.b.active_move.id == Move::KnockOff) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("ability: Sticky Hold")});
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(psychup_onHit) {
  ADV_UNUSED;
  c.b.at(source).boosts = c.b.at(t.pokemon).boosts;
  add(c, {part::text("-copyboost"), part::pokemon(c.b, source), part::pokemon(c.b, t.pokemon), part::from(effect(Move::PsychUp))});
  return relay::undefined;
}
// Cloud Nine and Air Lock stop the weather's effects while they are out.
ADV_HANDLER(weatherlock_onStart) {
  ADV_UNUSED;
  c.b.at(t.pokemon).ability_state.ending = false;
  return relay::undefined;
}
ADV_HANDLER(weatherlock_onEnd) {
  ADV_UNUSED;
  c.b.at(t.pokemon).ability_state.ending = true;
  each_event(c, WEATHER_CHANGE, c.effect);
  return relay::undefined;
}
// Trace takes the ability of a foe, drawn even when there is one.
ADV_HANDLER(trace_onStart) {
  ADV_UNUSED;
  const Ref foe = foe_of(c.b, t.pokemon);
  if (!foe) return relay::undefined;
  random(c.b, c.out, 1); // side.randomFoe(): sample of the foes
  if (c.b.at(foe).fainted) return relay::undefined;
  // Pokemon.setAbility(ability, target). Nothing listens for SetAbility in
  // Gen 3, and the new ability's onStart is not run.
  Pokemon& p = c.b.at(t.pokemon);
  const Ability old = p.ability, taken = c.b.at(foe).ability;
  single_event(c, callback("", "End"), effect(old), &p.ability_state, on(t.pokemon), foe);
  p.ability = taken;
  refresh_mask(c.b, p);
  p.ability_state = init_effect_state(c.b, taken != Ability::None, t.pokemon);
  add(c, {part::text("-ability"), part::pokemon(c.b, t.pokemon), part::ability(taken), part::ability(old),
          part::from(effect(Ability::Trace)), part::of(c.b, foe)});
  return relay::undefined;
}

// Reversal and Flail hit harder the less HP their user has, in 48ths.
ADV_HANDLER(lowhp_basePowerCallback) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  const int ratio = std::max(p.hp * 48 / p.maxhp, 1);
  return relay::number(ratio < 2 ? 200 : ratio < 5 ? 150 : ratio < 10 ? 100 : ratio < 17 ? 80 : ratio < 33 ? 40 : 20);
}
// Eruption and Water Spout: the move's power by the user's share of its HP,
// a fraction that getDamage floors.
ADV_HANDLER(fullhp_basePowerCallback) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  return relay::number(double(c.b.active_move.base_power) * p.hp / p.maxhp);
}
ADV_HANDLER(lowkick_basePowerCallback) {
  ADV_UNUSED;
  const int weight = std::max<int>(data(c.b.at(source).species).weight_hg, 1); // nothing changes a weight in Gen 3
  return relay::number(weight >= 2000 ? 120 : weight >= 1000 ? 100 : weight >= 500 ? 80 : weight >= 250 ? 60 : weight >= 100 ? 40 : 20);
}
ADV_HANDLER(facade_onBasePower) {
  ADV_UNUSED;
  const Condition status = c.b.at(t.pokemon).status;
  if (status != Condition::None && status != Condition::Slp) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(smellingsalts_basePowerCallback) {
  ADV_UNUSED;
  return relay::number(c.b.active_move.base_power * (c.b.at(source).status == Condition::Par ? 2 : 1));
}
ADV_HANDLER(smellingsalts_onHit) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).status == Condition::Par) cure_status(c, t.pokemon);
  return relay::undefined;
}
// How long Disable, Encore, Uproar and a binding move last: drawn.
ADV_HANDLER(two_to_five_durationCallback) {
  ADV_UNUSED;
  return relay::number(random(c.b, c.out, 2, 6));
}
ADV_HANDLER(three_to_six_durationCallback) {
  ADV_UNUSED;
  return relay::number(random(c.b, c.out, 3, 7));
}

ADV_HANDLER(struggle_onModifyMove) {
  ADV_UNUSED;
  c.b.active_move.type = Type::Unknown;
  return relay::undefined;
}
// Sleep Talk uses one of its user's other moves, drawn, as a move it called.
ADV_HANDLER(sleeptalk_onTry) {
  ADV_UNUSED;
  return c.b.at(t.pokemon).status == Condition::Slp ? relay::yes : relay::no;
}
ADV_HANDLER(sleeptalk_onTryHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  Pokemon& q = c.b.at(t.pokemon);
  (void)p;
  return !volatile_of(q, Condition::Choicelock) && !volatile_of(q, Condition::Encore) ? relay::yes : relay::no;
}
inline bool call_move(Ctx& c, Move id, Ref user, Effect by); // below
ADV_HANDLER(sleeptalk_onHit) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  std::array<int, MOVE_SLOTS> usable{};
  uint32_t n = 0;
  for (int i = 0; i < p.moves; i++) {
    const uint64_t flags = data(p.move_slots[i].id).flags;
    if (!(flags & flag(MoveFlag::Nosleeptalk)) && !(flags & flag(MoveFlag::Charge))) usable[n++] = i;
  }
  if (!n) return relay::no;
  const MoveSlot& slot = p.move_slots[usable[random(c.b, c.out, n)]];
  if (!slot.pp) {
    add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("nopp"), part::text(MOVE_NAMES[uint16_t(slot.id)].id)});
    return relay::undefined;
  }
  call_move(c, slot.id, t.pokemon, effect(Move::SleepTalk));
  return relay::undefined;
}
// Beat Up: one typeless hit of 10 power for each of the user's Pokémon that
// has no status, by that Pokémon's base Attack against the target's base
// Defense.
ADV_HANDLER(beatup_onModifyMove) {
  ADV_UNUSED;
  Battle::ActiveMove& move = c.b.active_move;
  add_volatile(c, t.pokemon, Condition::BeatUp);
  move.type = Type::Unknown;
  move.category = Category::Special;
  const Side& side = c.b.sides[t.pokemon.side];
  move.n_allies = 0;
  for (int position = 0; position < side.size; position++) {
    const uint8_t i = index_at(side, position);
    if (!side.pokemon[i].fainted && side.pokemon[i].status == Condition::None) move.allies[move.n_allies++] = i;
  }
  // With no one to hit the move has one hit, of no power, and fails.
  move.hits = move.n_allies;
  return relay::undefined;
}
ADV_HANDLER(beatup_basePowerCallback) {
  ADV_UNUSED;
  return c.b.active_move.n_allies ? relay::number(10) : relay::null;
}
ADV_HANDLER(beatup_condition_onModifySpA) {
  ADV_UNUSED;
  Battle::ActiveMove& move = c.b.active_move;
  // Under Beat Up Nicknames Mod the ally is not named.
  if (!move.n_allies) unported("Beat Up's Attack with no ally left");
  const uint8_t ally = move.allies[0];
  for (int i = 1; i < move.n_allies; i++) move.allies[i - 1] = move.allies[i];
  move.n_allies--;
  c.event.modifier = 1;
  return relay::number(data(c.b.sides[t.pokemon.side].pokemon[ally].set.species).base[uint8_t(Stat::Atk)]);
}
ADV_HANDLER(beatup_condition_onFoeModifySpD) {
  ADV_UNUSED;
  c.event.modifier = 1;
  return relay::number(data(c.b.at(t.pokemon).set.species).base[uint8_t(Stat::Def)]);
}
// Mean Look, Spider Web and Block trap the target for as long as their user
// stays in.
ADV_HANDLER(trapping_onHit) {
  ADV_UNUSED;
  return add_volatile(c, t.pokemon, Condition::Trapped, source, effect(c.b.active_move.id), Condition::Trapper) ? relay::yes : relay::no;
}
ADV_HANDLER(trapped_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("trapped")});
  return relay::undefined;
}
ADV_HANDLER(trapped_onTrapPokemon) {
  ADV_UNUSED;
  try_trap(c, t.pokemon);
  return relay::undefined;
}
// Destiny Bond takes whoever knocks its user out with a move.
ADV_HANDLER(destinybond_onPrepareHit) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::DestinyBond);
  return relay::undefined;
}
ADV_HANDLER(destinybond_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singlemove"), part::pokemon(c.b, t.pokemon), part::text("Destiny Bond")});
  return relay::undefined;
}
ADV_HANDLER(destinybond_condition_onFaint) {
  ADV_UNUSED;
  if (!source || !c.b.at(source).hp || !source_effect || source.side == t.pokemon.side) return relay::undefined;
  // Not for a Future Sight or a Doom Desire.
  if (source_effect.kind == EffectKind::Move && !(data(Move(source_effect.id)).flags & flag(MoveFlag::Futuremove))) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Destiny Bond")});
    faint(c, source, {}, {});
  }
  return relay::undefined;
}
ADV_HANDLER(destinybond_condition_onBeforeMove) {
  ADV_UNUSED;
  if (c.b.active_move.id == Move::DestinyBond) return relay::undefined;
  remove_volatile(c, t.pokemon, Condition::DestinyBond);
  return relay::undefined;
}
ADV_HANDLER(destinybond_condition_onMoveAborted) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::DestinyBond);
  return relay::undefined;
}

// Encore holds its target to the move it last used.
ADV_HANDLER(encore_condition_onStart) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  // Pokemon.getMoveData: the first slot that has the move.
  const MoveSlot* slot = nullptr;
  for (int i = 0; !slot && i < p.moves; i++) {
    if (p.move_slots[i].id == p.last_move) slot = &p.move_slots[i];
  }
  if (p.last_move == Move::None || (data(p.last_move).flags & flag(MoveFlag::Failencore)) || !slot || !slot->pp) return relay::no;
  c.effect_state->move = p.last_move;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Encore")});
  if (p.last_move == Move::Pursuit) add_volatile(c, t.pokemon, Condition::Pursuit, t.pokemon, effect(Move::Pursuit));
  return relay::undefined;
}
ADV_HANDLER(encore_condition_onOverrideAction) {
  ADV_UNUSED;
  const Move held = c.effect_state->move;
  return Move(source_effect.id) != held ? relay::number(uint16_t(held)) : relay::undefined;
}
ADV_HANDLER(encore_condition_onResidual) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  // The first slot that has the move, as getMoveData finds it.
  bool usable = false;
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].id != c.effect_state->move) continue;
    usable = p.move_slots[i].pp > 0;
    break;
  }
  if (!usable) remove_volatile(c, t.pokemon, Condition::Encore);
  return relay::undefined;
}
ADV_HANDLER(encore_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Encore")});
  return relay::undefined;
}
ADV_HANDLER(encore_condition_onDisableMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  const Move held = c.effect_state->move;
  bool has_move = false;
  for (int i = 0; i < p.moves; i++) has_move |= p.move_slots[i].id == held;
  if (held == Move::None || !has_move) return relay::undefined;
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].id != held) p.move_slots[i].disabled = Disabled::Yes;
  }
  return relay::undefined;
}
// Disable.
ADV_HANDLER(disable_onTryHit) {
  ADV_UNUSED;
  const Move last = c.b.at(t.pokemon).last_move;
  return last == Move::None || last == Move::Struggle ? relay::no : relay::undefined;
}
ADV_HANDLER(disable_condition_onStart) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  // A target that has already moved is disabled a turn longer.
  bool will_move = false;
  for (int i = 0; !p.fainted && i < c.b.queue.size; i++) {
    will_move |= c.b.queue.list[i].choice == Choice::Move && c.b.queue.list[i].pokemon == t.pokemon;
  }
  if (!will_move) c.effect_state->duration++;
  if (p.last_move == Move::None) return relay::no;
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].id != p.last_move) continue;
    if (!p.move_slots[i].pp) return relay::no;
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Disable"), slot_name(p, p.move_slots[i])});
    c.effect_state->move = p.last_move;
    return relay::undefined;
  }
  return relay::no;
}
ADV_HANDLER(disable_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Disable")});
  return relay::undefined;
}
ADV_HANDLER(disable_condition_onBeforeMove) {
  ADV_UNUSED;
  if (c.b.active_move.id != c.effect_state->move) return relay::undefined;
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("Disable"), part::move(c.b.active_move.id)});
  return relay::no;
}
ADV_HANDLER(disable_condition_onDisableMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  for (int i = 0; i < p.moves; i++) {
    if (p.move_slots[i].id == c.effect_state->move) p.move_slots[i].disabled = Disabled::Yes;
  }
  return relay::undefined;
}
// Yawn puts its target to sleep at the end of the next turn.
ADV_HANDLER(yawn_onTryHit) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).status != Condition::None || !run_status_immunity(c, t.pokemon, "slp")) return relay::no;
  return relay::undefined;
}
ADV_HANDLER(yawn_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Yawn"), part::of(c.b, source)});
  return relay::undefined;
}
ADV_HANDLER(yawn_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("move: Yawn"), part::text("[silent]")});
  try_set_status(c, t.pokemon, Condition::Slp, c.effect_state->source);
  return relay::undefined;
}
// Curse from a Ghost: half the user's HP for a quarter of the target's each
// turn.
ADV_HANDLER(curse_onHit) {
  ADV_UNUSED;
  direct_damage(c, double(c.b.at(source).maxhp) / 2, source, source);
  return relay::undefined;
}
ADV_HANDLER(curse_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Curse"), part::of(c.b, source)});
  return relay::undefined;
}
ADV_HANDLER(curse_condition_onResidual) {
  ADV_UNUSED;
  damage(c, double(c.b.at(t.pokemon).base_maxhp) / 4);
  return relay::undefined;
}
ADV_HANDLER(bellydrum_onHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (double(p.hp) <= double(p.maxhp) / 2 || p.boosts.atk >= 6 || p.maxhp == 1) return relay::no;
  direct_damage(c, double(p.maxhp) / 2);
  SparseBoosts up;
  up.set(0, 12);
  boost(c, up, t.pokemon, c.event.source);
  return relay::undefined;
}
// Pain Split: both end with the average of their HP (Pokemon.sethp keeps it
// between 1 and the maximum).
ADV_HANDLER(painsplit_onHit) {
  ADV_UNUSED;
  Pokemon& target = c.b.at(t.pokemon);
  Pokemon& user = c.b.at(source);
  const int average = std::max((target.hp + user.hp) / 2, 1);
  const auto set_hp = [](Pokemon& p, int hp) {
    if (!p.hp) return;
    p.hp = uint16_t(std::clamp(hp, 1, int(p.maxhp)));
  };
  set_hp(target, average);
  add(c, {part::text("-sethp"), part::pokemon(c.b, t.pokemon), part::health(c.b, t.pokemon), part::from(effect(Move::PainSplit)),
          part::text("[silent]")});
  set_hp(user, average);
  add(c, {part::text("-sethp"), part::pokemon(c.b, source), part::health(c.b, source), part::from(effect(Move::PainSplit))});
  return relay::undefined;
}
ADV_HANDLER(fakeout_onTry) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).active_move_actions > 1) {
    add(c, {part::text("-hint"), part::text("Fake Out only works on your first turn out.")});
    return relay::no;
  }
  return relay::undefined;
}
// A move that charges for a turn. The volatile under its own id says it has
// charged; `twoturnmove` holds the Pokémon to it.
inline Relay charge(Ctx& c, Ref attacker, Ref defender) {
  const Move id = c.b.active_move.id;
  if (remove_volatile(c, attacker, condition_of(id))) return relay::undefined;
  add(c, {part::text("-prepare"), part::pokemon(c.b, attacker), part::move(id)});
  if (id == Move::SolarBeam) {
    const Condition weather = effective_weather(c.b);
    if (weather == Condition::Sunnyday || weather == Condition::Desolateland) {
      attr_last_move(c, "[still]");
      add_move(c, {part::text("-anim"), part::pokemon(c.b, attacker), part::move(id), part::pokemon(c.b, defender)});
      return relay::undefined;
    }
  }
  if (id == Move::SkullBash) {
    SparseBoosts up;
    up.set(1, 1);
    boost(c, up, attacker, attacker, effect(id));
  }
  // ChargeMove: nothing in Gen 3 listens.
  add_volatile(c, attacker, Condition::Twoturnmove, defender);
  return relay::null;
}
ADV_HANDLER(charge_onTryMove) {
  ADV_UNUSED;
  return charge(c, t.pokemon, source);
}
ADV_HANDLER(solarbeam_onBasePower) {
  ADV_UNUSED;
  const Condition weather = effective_weather(c.b);
  if (weather == Condition::Raindance || weather == Condition::Primordialsea || weather == Condition::Sandstorm ||
      weather == Condition::Hail || weather == Condition::Snowscape) {
    chain_modify(c, 0.5);
  }
  return relay::undefined;
}
ADV_HANDLER(twoturnmove_onStart) {
  ADV_UNUSED;
  const Move id = Move(source_effect.id);
  c.effect_state->move = id;
  add_volatile(c, t.pokemon, condition_of(id));
  // A move that another move called is aimed again, and at a foe that has
  // fainted the aim is drawn from the one there is.
  if (c.b.active_move.source_effect && data(id).target != Target::Self && c.b.at(source).fainted) random(c.b, c.out, 1);
  attr_last_move(c, "[still]");
  run_event(c, events::PREPARE_HIT, on(t.pokemon), source, source_effect);
  return relay::undefined;
}
ADV_HANDLER(twoturnmove_onEnd) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, condition_of(c.effect_state->move));
  return relay::undefined;
}
ADV_HANDLER(twoturnmove_onLockMove) {
  ADV_UNUSED;
  return relay::number(uint16_t(c.effect_state->move));
}
ADV_HANDLER(twoturnmove_onMoveAborted) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::Twoturnmove);
  return relay::undefined;
}
// In the air, underground or underwater: only some moves reach.
inline Relay out_of_reach(const Ctx& c, std::initializer_list<Move> reach) {
  for (const Move m : reach) {
    if (c.b.active_move.id == m) return relay::undefined;
  }
  return relay::no;
}
ADV_HANDLER(fly_condition_onInvulnerability) {
  ADV_UNUSED;
  return out_of_reach(c, {Move::Gust, Move::Twister, Move::SkyUppercut, Move::Thunder});
}
ADV_HANDLER(dig_condition_onInvulnerability) {
  ADV_UNUSED;
  return out_of_reach(c, {Move::Earthquake, Move::Magnitude});
}
ADV_HANDLER(dive_condition_onInvulnerability) {
  ADV_UNUSED;
  return out_of_reach(c, {Move::Surf, Move::Whirlpool});
}
ADV_HANDLER(fly_condition_onSourceModifyDamage) {
  ADV_UNUSED;
  if (c.b.active_move.id == Move::Gust || c.b.active_move.id == Move::Twister) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(bounce_condition_onSourceBasePower) {
  ADV_UNUSED;
  if (c.b.active_move.id == Move::Gust || c.b.active_move.id == Move::Twister) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(dig_condition_onSourceModifyDamage) {
  ADV_UNUSED;
  if (c.b.active_move.id == Move::Earthquake || c.b.active_move.id == Move::Magnitude) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(dive_condition_onSourceModifyDamage) {
  ADV_UNUSED;
  if (c.b.active_move.id == Move::Surf || c.b.active_move.id == Move::Whirlpool) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(underground_onImmunity) {
  ADV_UNUSED;
  return value.text == "sandstorm" || value.text == "hail" ? relay::no : relay::undefined;
}
// Outrage, Thrash, Petal Dance: two or three turns, and then confusion.
// `time` is Showdown's trueDuration.
ADV_HANDLER(lockedmove_onStart) {
  ADV_UNUSED;
  c.effect_state->time = int8_t(random(c.b, c.out, 2, 4));
  c.effect_state->move = Move(source_effect.id);
  return relay::undefined;
}
ADV_HANDLER(lockedmove_onRestart) {
  ADV_UNUSED;
  if (c.effect_state->time >= 2) c.effect_state->duration = 2;
  return relay::undefined;
}
ADV_HANDLER(lockedmove_onResidual) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  // Asleep, the volatile is deleted: nothing is run for its end.
  if (p.status == Condition::Slp) {
    if (Volatile* v = volatile_of(p, Condition::Lockedmove)) v->id = Condition::None;
  }
  c.effect_state->time--;
  return relay::undefined;
}
ADV_HANDLER(lockedmove_onEnd) {
  ADV_UNUSED;
  if (c.effect_state->time > 1) return relay::undefined;
  add_volatile(c, t.pokemon, Condition::Confusion);
  return relay::undefined;
}
ADV_HANDLER(lockedmove_onLockMove) {
  ADV_UNUSED;
  return relay::number(uint16_t(c.effect_state->move));
}
// Hyper Beam: the next turn is spent.
ADV_HANDLER(mustrecharge_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-mustrecharge"), part::pokemon(c.b, t.pokemon)});
  return relay::undefined;
}
ADV_HANDLER(mustrecharge_onBeforeMove) {
  ADV_UNUSED;
  add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("recharge")});
  remove_volatile(c, t.pokemon, Condition::Mustrecharge);
  return relay::null;
}
ADV_HANDLER(mustrecharge_onLockMove) {
  ADV_UNUSED;
  return relay::text("recharge");
}
// A move's id as Showdown has it: every Hidden Power is `hiddenpower`.
inline std::string_view showdown_id(Move m) {
  return MOVE_NAMES[uint16_t(data(m).real_move != Move::None ? data(m).real_move : m)].id;
}
ADV_HANDLER(spite_onHit) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  const int roll = int(random(c.b, c.out, 2, 6));
  if (p.last_move != Move::None && deduct_pp(p, p.last_move, roll)) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Spite"), part::text(showdown_id(p.last_move)),
            part::number(roll)});
    return relay::undefined;
  }
  return relay::no;
}
// Stockpile, and the two moves that spend it.
ADV_HANDLER(stockpile_onTry) {
  ADV_UNUSED;
  const Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::Stockpile);
  return v && v->state.layers >= 3 ? relay::no : relay::undefined;
}
inline void stockpile_line(Ctx& c, Ref r, int layers) {
  static constexpr std::string_view TEXT[] = {"stockpile0", "stockpile1", "stockpile2", "stockpile3"};
  add(c, {part::text("-start"), part::pokemon(c.b, r), part::text(TEXT[layers])});
}
ADV_HANDLER(stockpile_condition_onStart) {
  ADV_UNUSED;
  c.effect_state->layers = 1;
  stockpile_line(c, t.pokemon, 1);
  return relay::undefined;
}
ADV_HANDLER(stockpile_condition_onRestart) {
  ADV_UNUSED;
  if (c.effect_state->layers >= 3) return relay::no;
  c.effect_state->layers++;
  stockpile_line(c, t.pokemon, c.effect_state->layers);
  return relay::undefined;
}
ADV_HANDLER(stockpile_condition_onEnd) {
  ADV_UNUSED;
  c.effect_state->layers = 0;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Stockpile")});
  return relay::undefined;
}
ADV_HANDLER(needs_stockpile_onTry) {
  ADV_UNUSED;
  return volatile_of(c.b.at(t.pokemon), Condition::Stockpile) ? relay::yes : relay::no;
}
ADV_HANDLER(swallow_onHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  const Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::Stockpile);
  const int layers = v && v->state.layers ? v->state.layers : 1;
  static constexpr double PART[] = {0.25, 0.5, 1};
  const bool success = heal(c, modify(p.maxhp, PART[layers - 1])) != 0;
  if (!success) add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("heal")});
  remove_volatile(c, t.pokemon, Condition::Stockpile);
  return success ? relay::yes : relay::null;
}
ADV_HANDLER(spitup_basePowerCallback) {
  ADV_UNUSED;
  const Volatile* v = volatile_of(c.b.at(t.pokemon), Condition::Stockpile);
  if (!v || !v->state.layers) return relay::no;
  return relay::number(v->state.layers * 100);
}
ADV_HANDLER(spitup_onAfterMove) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::Stockpile);
  return relay::undefined;
}
ADV_HANDLER(dreameater_onTryImmunity) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  return p.status == Condition::Slp && !volatile_of(p, Condition::Substitute) ? relay::yes : relay::no;
}
// Moonlight, Morning Sun, Synthesis: how much depends on the weather.
ADV_HANDLER(weather_heal_onHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  const Condition weather = effective_weather(c.b);
  if (weather == Condition::Sunnyday || weather == Condition::Desolateland) {
    heal(c, double(p.maxhp) * 2 / 3);
  } else if (weather == Condition::Raindance || weather == Condition::Primordialsea || weather == Condition::Sandstorm ||
             weather == Condition::Hail) {
    heal(c, double(p.base_maxhp) / 4);
  } else {
    heal(c, double(p.base_maxhp) / 2);
  }
  return relay::undefined;
}
ADV_HANDLER(payday_onHit) {
  ADV_UNUSED;
  add(c, {part::text("-fieldactivate"), part::text("move: Pay Day")});
  return relay::undefined;
}
ADV_HANDLER(snore_onTry) {
  ADV_UNUSED;
  return c.b.at(t.pokemon).status == Condition::Slp ? relay::yes : relay::no;
}
ADV_HANDLER(psywave_damageCallback) {
  ADV_UNUSED;
  return relay::number(double(random(c.b, c.out, 50, 151)) * c.b.at(t.pokemon).level / 100);
}
ADV_HANDLER(magnitude_onModifyMove) {
  ADV_UNUSED;
  const uint32_t i = random(c.b, c.out, 100);
  static constexpr struct { uint32_t below; uint8_t magnitude, power; } TABLE[] = {
      {5, 4, 10}, {15, 5, 30}, {35, 6, 50}, {65, 7, 70}, {85, 8, 90}, {95, 9, 110}, {100, 10, 150}};
  for (const auto& row : TABLE) {
    if (i >= row.below) continue;
    c.b.active_move.magnitude = row.magnitude;
    c.b.active_move.base_power = row.power;
    break;
  }
  return relay::undefined;
}
ADV_HANDLER(magnitude_onUseMoveMessage) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Magnitude"), part::number(c.b.active_move.magnitude)});
  return relay::undefined;
}
// Teleport's onTry is the constant false.
ADV_HANDLER(teleport_onTry) {
  ADV_UNUSED;
  return relay::no;
}
ADV_HANDLER(revenge_basePowerCallback) {
  ADV_UNUSED;
  // Any hit from the target this turn that did damage.
  const Pokemon::AttackedBy& hit = c.b.at(t.pokemon).attacked_by[source.index];
  const bool damaged_by_target = hit.order && hit.damaged_this_turn;
  return relay::number(c.b.active_move.base_power * (damaged_by_target ? 2 : 1));
}
ADV_HANDLER(falseswipe_onDamage) {
  ADV_UNUSED;
  const uint16_t hp = c.b.at(t.pokemon).hp;
  return value.number >= hp ? relay::number(hp - 1) : relay::undefined;
}
ADV_HANDLER(present_onModifyMove) {
  ADV_UNUSED;
  const uint32_t rand = random(c.b, c.out, 10);
  if (rand < 2) c.b.active_move.heals = true;
  else c.b.active_move.base_power = rand < 6 ? 40 : rand < 9 ? 80 : 120;
  return relay::undefined;
}
ADV_HANDLER(splash_onTry) {
  ADV_UNUSED;
  return relay::undefined; // Gravity is not of Gen 3
}
ADV_HANDLER(splash_onTryHit) {
  ADV_UNUSED;
  add(c, {part::text("-nothing")});
  return relay::undefined;
}
ADV_HANDLER(superfang_damageCallback) {
  ADV_UNUSED;
  return relay::number(std::max(c.b.at(source).hp / 2, 1));
}
// Whether two moves are one to Showdown: every Hidden Power is.
inline bool same_move(Move a, Move b) {
  const auto real = [](Move m) { return data(m).real_move != Move::None ? data(m).real_move : m; };
  return real(a) == real(b);
}
// Pokemon.disableMove.
inline void disable_move(Pokemon& p, Move id, bool hidden = false) {
  for (int i = 0; i < p.moves; i++) {
    MoveSlot& slot = p.move_slots[i];
    if (same_move(slot.id, id) && slot.disabled != Disabled::Yes) slot.disabled = hidden ? Disabled::Hidden : Disabled::Yes;
  }
}
// Foresight and Odor Sleuth: their onTryHit is for Miracle Eye, not of Gen 3.
ADV_HANDLER(foresight_onTryHit) {
  ADV_UNUSED;
  return relay::undefined;
}
ADV_HANDLER(foresight_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Foresight")});
  return relay::undefined;
}
// onNegateImmunity(pokemon, type): the type is the move's in use.
ADV_HANDLER(foresight_condition_onNegateImmunity) {
  ADV_UNUSED;
  const Type type = c.b.active_move.type;
  if (has_type(c.b.at(t.pokemon), Type::Ghost) && (type == Type::Normal || type == Type::Fighting)) return relay::no;
  return relay::undefined;
}
ADV_HANDLER(foresight_condition_onModifyBoost) {
  ADV_UNUSED;
  SparseBoosts boosts = value.boosts;
  if (boosts.has(6) && boosts.value[6] > 0) boosts.set(6, 0);
  return relay::boosts(boosts);
}
// Lock-On and Mind Reader: the user's next move at that target hits.
ADV_HANDLER(lockon_onTryHit) {
  ADV_UNUSED;
  return volatile_of(c.b.at(source), Condition::LockOn) ? relay::no : relay::undefined;
}
ADV_HANDLER(lockon_onHit) {
  ADV_UNUSED;
  add_volatile(c, source, Condition::LockOn, t.pokemon);
  add(c, {part::text("-activate"), part::pokemon(c.b, source), part::text("move: Lock-On"), part::of(c.b, t.pokemon)});
  return relay::undefined;
}
ADV_HANDLER(mindreader_onHit) {
  ADV_UNUSED;
  add_volatile(c, source, Condition::LockOn, t.pokemon);
  add(c, {part::text("-activate"), part::pokemon(c.b, source), part::text("move: Mind Reader"), part::of(c.b, t.pokemon)});
  return relay::undefined;
}
ADV_HANDLER(lockon_condition_onSourceInvulnerability) {
  ADV_UNUSED;
  if (source == c.effect_state->target_pokemon && t.pokemon == c.effect_state->source) return relay::number(0);
  return relay::undefined;
}
ADV_HANDLER(lockon_condition_onSourceAccuracy) {
  ADV_UNUSED;
  if (source == c.effect_state->target_pokemon && t.pokemon == c.effect_state->source) return relay::yes;
  return relay::undefined;
}
ADV_HANDLER(nightmare_condition_onStart) {
  ADV_UNUSED;
  if (c.b.at(t.pokemon).status != Condition::Slp) return relay::no;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Nightmare")});
  return relay::undefined;
}
ADV_HANDLER(nightmare_condition_onResidual) {
  ADV_UNUSED;
  damage(c, double(c.b.at(t.pokemon).base_maxhp) / 4);
  return relay::undefined;
}
// Imprison: the foes cannot use the moves its user knows.
ADV_HANDLER(imprison_onTryHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  const Ref foe = foe_of(c.b, t.pokemon);
  if (!foe) return relay::no;
  const Pokemon& other = c.b.at(foe);
  for (int i = 0; i < p.moves; i++) {
    for (int j = 0; j < other.moves; j++) {
      if (same_move(p.move_slots[i].id, other.move_slots[j].id)) return relay::undefined;
    }
  }
  return relay::no;
}
ADV_HANDLER(imprison_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Imprison")});
  return relay::undefined;
}
ADV_HANDLER(imprison_condition_onFoeDisableMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  const Pokemon& user = c.b.at(c.effect_state->source);
  for (int i = 0; i < user.moves; i++) disable_move(p, user.move_slots[i].id, true);
  p.maybe_disabled = true;
  return relay::undefined;
}
ADV_HANDLER(imprison_condition_onFoeBeforeMove) {
  ADV_UNUSED;
  const Move id = c.b.active_move.id;
  if (id == Move::Struggle) return relay::undefined;
  const Pokemon& user = c.b.at(c.effect_state->source);
  for (int i = 0; i < user.moves; i++) {
    if (!same_move(user.move_slots[i].id, id)) continue;
    add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("move: Imprison"), part::move(id)});
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(ingrain_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Ingrain")});
  return relay::undefined;
}
ADV_HANDLER(ingrain_condition_onResidual) {
  ADV_UNUSED;
  heal(c, double(c.b.at(t.pokemon).base_maxhp) / 16);
  return relay::undefined;
}
ADV_HANDLER(ingrain_condition_onTrapPokemon) {
  ADV_UNUSED;
  try_trap(c, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(ingrain_condition_onDragOut) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Ingrain")});
  return relay::null;
}
// Minimize and Defense Curl are not started again.
ADV_HANDLER(null_onRestart) {
  ADV_UNUSED;
  return relay::null;
}
ADV_HANDLER(minimize_condition_onSourceModifyDamage) {
  ADV_UNUSED;
  if (data(c.b.active_move.id).flags & flag(MoveFlag::Minimize)) chain_modify(c, 2);
  return relay::undefined;
}
ADV_HANDLER(mudsport_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Mud Sport")});
  return relay::undefined;
}
ADV_HANDLER(mudsport_condition_onAnyBasePower) {
  ADV_UNUSED;
  if (c.b.active_move.type == Type::Electric) chain_modify(c, 0.5);
  return relay::undefined;
}
ADV_HANDLER(watersport_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Water Sport")});
  return relay::undefined;
}
ADV_HANDLER(watersport_condition_onAnyBasePower) {
  ADV_UNUSED;
  if (c.b.active_move.type == Type::Fire) chain_modify(c, 0.5);
  return relay::undefined;
}
ADV_HANDLER(torment_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Torment")});
  return relay::undefined;
}
ADV_HANDLER(torment_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Torment")});
  return relay::undefined;
}
ADV_HANDLER(torment_condition_onDisableMove) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.last_move != Move::None && p.last_move != Move::Struggle) disable_move(p, p.last_move);
  return relay::undefined;
}
// Rage: Attack rises each time its user is hit, until it uses another move.
ADV_HANDLER(rage_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singlemove"), part::pokemon(c.b, t.pokemon), part::text("Rage")});
  return relay::undefined;
}
ADV_HANDLER(rage_condition_onHit) {
  ADV_UNUSED;
  if (t.pokemon != source && c.b.active_move.category != Category::Status) {
    SparseBoosts up;
    up.set(0, 1);
    boost(c, up, t.pokemon, source);
  }
  return relay::undefined;
}
ADV_HANDLER(rage_condition_onBeforeMove) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::Rage);
  return relay::undefined;
}
// Grudge: the move that makes its user faint loses all its PP.
ADV_HANDLER(grudge_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singlemove"), part::pokemon(c.b, t.pokemon), part::text("Grudge")});
  return relay::undefined;
}
ADV_HANDLER(grudge_condition_onFaint) {
  ADV_UNUSED;
  if (!source || !c.b.at(source).hp || !source_effect) return relay::undefined;
  Pokemon& attacker = c.b.at(source);
  if (source_effect.kind != EffectKind::Move || (data(Move(source_effect.id)).flags & flag(MoveFlag::Futuremove)) ||
      attacker.last_move == Move::None) {
    return relay::undefined;
  }
  for (int i = 0; i < attacker.moves; i++) {
    if (attacker.move_slots[i].id != attacker.last_move) continue;
    attacker.move_slots[i].pp = 0;
    add(c, {part::text("-activate"), part::pokemon(c.b, source), part::text("move: Grudge"), part::move(attacker.last_move)});
  }
  return relay::undefined;
}
ADV_HANDLER(grudge_condition_onBeforeMove) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::Grudge);
  return relay::undefined;
}
// Attract: half the time the Pokémon cannot move, while the one it is
// attracted to stays.
inline bool opposite_genders(const Pokemon& a, const Pokemon& b) {
  return (a.gender == Gender::Male && b.gender == Gender::Female) || (a.gender == Gender::Female && b.gender == Gender::Male);
}
ADV_HANDLER(attract_onTryImmunity) {
  ADV_UNUSED;
  return opposite_genders(c.b.at(t.pokemon), c.b.at(source)) ? relay::yes : relay::no;
}
ADV_HANDLER(attract_condition_onStart) {
  ADV_UNUSED;
  if (!opposite_genders(c.b.at(t.pokemon), c.b.at(source))) return relay::no;
  // Nothing in Gen 3 listens for Attract.
  if (source_effect == effect(Ability::CuteCharm)) {
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Attract"), part::from(source_effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Attract")});
  }
  return relay::undefined;
}
ADV_HANDLER(attract_condition_onUpdate) {
  ADV_UNUSED;
  const Ref by = c.effect_state->source;
  if (by && !c.b.at(by).is_active && volatile_of(c.b.at(t.pokemon), Condition::Attract)) remove_volatile(c, t.pokemon, Condition::Attract);
  return relay::undefined;
}
ADV_HANDLER(attract_condition_onBeforeMove) {
  ADV_UNUSED;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Attract"), part::of(c.b, c.effect_state->source)});
  if (random(c.b, c.out, 2) < 1) { // randomChance(1, 2)
    add(c, {part::text("cant"), part::pokemon(c.b, t.pokemon), part::text("Attract")});
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(attract_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Attract"), part::text("[silent]")});
  return relay::undefined;
}
// Jump Kick and High Jump Kick: a kick that fails hurts its user by half of
// what it would have dealt, unless the target is immune to Fighting.
ADV_HANDLER(crash_onMoveFail) {
  ADV_UNUSED;
  if (!run_immunity(c, t.pokemon, false)) return relay::undefined;
  const Relay would = get_damage(c, source, t.pokemon, false);
  if (would.kind != Relay::Kind::Number) unported("a crash with no damage to halve");
  const double amount = std::clamp(std::floor(would.number / 2), 1.0, std::floor(double(c.b.at(t.pokemon).maxhp) / 2));
  damage(c, amount, source, source, effect(c.b.active_move.id));
  return relay::undefined;
}
ADV_HANDLER(triplekick_basePowerCallback) {
  ADV_UNUSED;
  return relay::number(10 * c.b.active_move.hit);
}
// Rollout and Ice Ball: five turns, each hit twice the last. `counter` is
// Showdown's hitCount and contactHitCount, which are always equal, so
// `rolloutstorage` is never reached.
inline Relay rolling_base_power(Ctx& c, Ref user, Condition rolling) {
  Pokemon& p = c.b.at(user);
  double bp = c.b.active_move.base_power;
  if (Volatile* v = volatile_of(p, rolling)) {
    if (v->state.counter) bp *= double(1u << v->state.counter);
    if (p.status != Condition::Slp) {
      v->state.counter++;
      if (v->state.counter < 5) v->state.duration = 2;
    }
  }
  if (volatile_of(p, Condition::DefenseCurl)) bp *= 2;
  return relay::number(bp);
}
inline Relay start_rolling(Ctx& c, Ref user, Ref target, Condition rolling) {
  if (volatile_of(c.b.at(user), rolling) || c.b.at(user).status == Condition::Slp || !target) return relay::undefined;
  add_volatile(c, user, rolling);
  return relay::undefined;
}
ADV_HANDLER(rollout_basePowerCallback) {
  ADV_UNUSED;
  return rolling_base_power(c, t.pokemon, Condition::Rollout);
}
ADV_HANDLER(iceball_basePowerCallback) {
  ADV_UNUSED;
  return rolling_base_power(c, t.pokemon, Condition::IceBall);
}
ADV_HANDLER(rollout_onModifyMove) {
  ADV_UNUSED;
  return start_rolling(c, t.pokemon, source, Condition::Rollout);
}
ADV_HANDLER(iceball_onModifyMove) {
  ADV_UNUSED;
  return start_rolling(c, t.pokemon, source, Condition::IceBall);
}
ADV_HANDLER(rolling_onAfterMove) {
  ADV_UNUSED;
  return relay::undefined; // five hits and fewer than five contact hits: never
}
ADV_HANDLER(rollout_condition_onLockMove) {
  ADV_UNUSED;
  return relay::number(uint16_t(Move::Rollout));
}
ADV_HANDLER(iceball_condition_onLockMove) {
  ADV_UNUSED;
  return relay::number(uint16_t(Move::IceBall));
}
ADV_HANDLER(rolling_condition_onStart) {
  ADV_UNUSED;
  c.effect_state->counter = 0;
  return relay::undefined;
}
ADV_HANDLER(rollout_condition_onResidual) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.last_move == Move::Struggle) {
    if (Volatile* v = volatile_of(p, Condition::Rollout)) v->id = Condition::None;
  }
  return relay::undefined;
}
ADV_HANDLER(iceball_condition_onResidual) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.last_move == Move::Struggle) {
    if (Volatile* v = volatile_of(p, Condition::IceBall)) v->id = Condition::None;
  }
  return relay::undefined;
}
// Fury Cutter doubles with each hit in a row. `counter` is the multiplier.
ADV_HANDLER(furycutter_basePowerCallback) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (!volatile_of(p, Condition::FuryCutter) || c.b.active_move.hit == 1) add_volatile(c, t.pokemon, Condition::FuryCutter);
  const Volatile* v = volatile_of(p, Condition::FuryCutter);
  return relay::number(std::clamp(c.b.active_move.base_power * (v ? int(v->state.counter) : 1), 1, 160));
}
ADV_HANDLER(furycutter_onHit) {
  ADV_UNUSED;
  add_volatile(c, source, Condition::FuryCutter);
  return relay::undefined;
}
ADV_HANDLER(furycutter_condition_onStart) {
  ADV_UNUSED;
  c.effect_state->counter = 1;
  return relay::undefined;
}
ADV_HANDLER(furycutter_condition_onRestart) {
  ADV_UNUSED;
  if (c.effect_state->counter < 16) c.effect_state->counter = uint16_t(c.effect_state->counter << 1);
  c.effect_state->duration = 2;
  return relay::undefined;
}
// Uproar: no one sleeps while it lasts.
ADV_HANDLER(uproar_onTryHit) {
  ADV_UNUSED;
  // The target's side first, then the other.
  const Ref mine{t.pokemon.side, c.b.sides[t.pokemon.side].active};
  const Ref theirs{uint8_t(t.pokemon.side ^ 1), c.b.sides[t.pokemon.side ^ 1].active};
  for (const Ref r : {mine, theirs}) {
    if (r.index != 0xFF && c.b.at(r).status == Condition::Slp) cure_status(c, r);
  }
  return relay::undefined;
}
ADV_HANDLER(uproar_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Uproar")});
  return relay::undefined;
}
ADV_HANDLER(uproar_condition_onResidual) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.last_move == Move::Struggle) {
    if (Volatile* v = volatile_of(p, Condition::Uproar)) v->id = Condition::None;
  }
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Uproar"), part::text("[upkeep]")});
  return relay::undefined;
}
ADV_HANDLER(uproar_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Uproar")});
  return relay::undefined;
}
ADV_HANDLER(uproar_condition_onLockMove) {
  ADV_UNUSED;
  return relay::number(uint16_t(Move::Uproar));
}
ADV_HANDLER(uproar_condition_onAnySetStatus) {
  ADV_UNUSED;
  if (value.text != "slp") return relay::undefined;
  if (t.pokemon == c.effect_state->target_pokemon) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("slp"), part::text("[from] Uproar"), part::text("[msg]")});
  } else {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("slp"), part::text("[from] Uproar")});
  }
  return relay::null;
}
// Bide: two turns of taking hits, then twice the damage back at whoever
// dealt the last of it. `damage` is Showdown's totalDamage.
ADV_HANDLER(bide_beforeMoveCallback) {
  ADV_UNUSED;
  return volatile_of(c.b.at(t.pokemon), Condition::Bide) ? relay::yes : relay::undefined;
}
ADV_HANDLER(bide_condition_onLockMove) {
  ADV_UNUSED;
  return relay::number(uint16_t(Move::Bide));
}
ADV_HANDLER(bide_condition_onStart) {
  ADV_UNUSED;
  c.effect_state->damage = 0;
  c.effect_state->last_damage_source = Ref{};
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("move: Bide")});
  return relay::undefined;
}
ADV_HANDLER(bide_condition_onDamage) {
  ADV_UNUSED;
  // A move's damage, the hit a confused Pokémon gives itself included.
  const bool by_move = (source_effect.kind == EffectKind::Move && source_effect) || source_effect == effect(Special::Confused);
  if (!by_move || !source) return relay::undefined;
  c.effect_state->damage = uint16_t(c.effect_state->damage + value.number);
  c.effect_state->last_damage_source = source;
  return relay::undefined;
}
ADV_HANDLER(bide_condition_onBeforeMove) {
  ADV_UNUSED;
  if (c.effect_state->duration != 1) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Bide")});
    return relay::undefined;
  }
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("move: Bide")});
  Ref target = c.effect_state->last_damage_source;
  if (!c.effect_state->damage || !target) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon)});
    return relay::no;
  }
  // One that has left is replaced by the foe that is there now.
  if (!c.b.at(target).is_active) {
    const Side& foes = c.b.sides[t.pokemon.side ^ 1];
    if (foes.active == 0xFF) {
      add(c, {part::text("-miss"), part::pokemon(c.b, t.pokemon)});
      return relay::no;
    }
    target = {uint8_t(t.pokemon.side ^ 1), foes.active};
  }
  // The release is a made-up move: Normal, physical, 100 accurate, with
  // contact, stopped by Protect, and of twice the damage taken.
  const uint32_t twice = uint32_t(c.effect_state->damage) * 2;
  c.b.active_move = {.id = Move::Bide, .type = Type::Normal, .category = Category::Physical, .accuracy = 100,
                     .target = Target::Normal, .fixed_damage = twice};
  try_move_hit(c, target, t.pokemon);
  remove_volatile(c, t.pokemon, Condition::Bide);
  return relay::no;
}
ADV_HANDLER(bide_condition_onMoveAborted) {
  ADV_UNUSED;
  remove_volatile(c, t.pokemon, Condition::Bide);
  return relay::undefined;
}
ADV_HANDLER(bide_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("move: Bide"), part::text("[silent]")});
  return relay::undefined;
}
// The moves that change a Pokémon's type.
inline void typechange_line(Ctx& c, Ref r, Type type) {
  add(c, {part::text("-start"), part::pokemon(c.b, r), part::text("typechange"), part::text(TYPE_NAMES[uint8_t(type)].name)});
}
// Conversion: the type of one of the user's moves, drawn from those it does
// not already have. Hidden Power counts as Normal, and a type as often as
// it has moves of it.
ADV_HANDLER(conversion_onHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  std::array<Type, MOVE_SLOTS> possible{};
  uint32_t n = 0;
  for (int i = 0; i < p.moves; i++) {
    const Move id = p.move_slots[i].id;
    if (id != Move::Curse && !has_type(p, data(id).type)) possible[n++] = data(id).type;
  }
  if (!n) return relay::no;
  const Type type = possible[random(c.b, c.out, n)];
  set_type(c, t.pokemon, type);
  typechange_line(c, t.pokemon, type);
  return relay::undefined;
}
// Conversion 2: a type that resists the move the target last used.
ADV_HANDLER(conversion2_onHit) {
  ADV_UNUSED;
  const Pokemon& other = c.b.at(t.pokemon);
  if (other.last_move_used == Move::None) return relay::no;
  const Type attack = other.last_move_used == Move::Struggle ? Type::Normal : other.last_move_used_type;
  std::array<Type, 18> possible{};
  uint32_t n = 0;
  if (attack != Type::None && attack != Type::Unknown) {
    for (int type = int(Type::Electric); type <= int(Type::Water); type++) {
      const int taken = damage_taken(Type(type), attack);
      if (taken == 2 || taken == 3) possible[n++] = Type(type);
    }
  }
  if (!n) return relay::no;
  const Type type = possible[random(c.b, c.out, n)];
  set_type(c, source, type);
  typechange_line(c, source, type);
  return relay::undefined;
}
ADV_HANDLER(camouflage_onHit) {
  ADV_UNUSED;
  if (has_type(c.b.at(t.pokemon), Type::Normal)) return relay::no;
  set_type(c, t.pokemon, Type::Normal);
  typechange_line(c, t.pokemon, Type::Normal);
  return relay::undefined;
}
ADV_HANDLER(recycle_onHit) {
  ADV_UNUSED;
  Pokemon& p = c.b.at(t.pokemon);
  if (p.item != Item::None || p.last_item == Item::None) return relay::no;
  const Item item = p.last_item;
  p.last_item = Item::None;
  add(c, {part::text("-item"), part::pokemon(c.b, t.pokemon), part::item(item), part::from(effect(Move::Recycle))});
  set_item(c, t.pokemon, item, source, effect(Move::Recycle));
  return relay::undefined;
}
// Trick: the two trade items, unless either is kept where it is.
ADV_HANDLER(trick_onTryImmunity) {
  ADV_UNUSED;
  return c.b.at(t.pokemon).ability == Ability::StickyHold ? relay::no : relay::yes;
}
ADV_HANDLER(trick_onHit) {
  ADV_UNUSED;
  const Effect me = effect(Move::Trick);
  const Taken yours = take_item(c, t.pokemon, source);
  const Taken mine = take_item(c, source);
  const auto put_back = [&] {
    if (yours.item != Item::None) c.b.at(t.pokemon).item = yours.item;
    if (mine.item != Item::None) c.b.at(source).item = mine.item;
    refresh_mask(c.b, c.b.at(t.pokemon));
    refresh_mask(c.b, c.b.at(source));
    return relay::no;
  };
  if (yours.refused || mine.refused || (yours.item == Item::None && mine.item == Item::None)) return put_back();
  // Each item is asked whether it may go to its new holder.
  const auto may_go = [&](Item item, Ref to, Ref from) {
    EffectState none{};
    return single_event(c, callback("", "TakeItem"), effect(item), &none, on(to), from, me, relay::yes).truthy();
  };
  if ((mine.item != Item::None && !may_go(mine.item, t.pokemon, source)) ||
      (yours.item != Item::None && !may_go(yours.item, source, t.pokemon))) {
    return put_back();
  }
  add(c, {part::text("-activate"), part::pokemon(c.b, source), part::text("move: Trick"), part::of(c.b, t.pokemon)});
  if (mine.item != Item::None) {
    set_item(c, t.pokemon, mine.item);
    add(c, {part::text("-item"), part::pokemon(c.b, t.pokemon), part::item(mine.item), part::from(me)});
  } else {
    add(c, {part::text("-enditem"), part::pokemon(c.b, t.pokemon), part::item(yours.item), part::text("[silent]"), part::from(me)});
  }
  if (yours.item != Item::None) {
    set_item(c, source, yours.item);
    add(c, {part::text("-item"), part::pokemon(c.b, source), part::item(yours.item), part::from(me)});
  } else {
    add(c, {part::text("-enditem"), part::pokemon(c.b, source), part::item(mine.item), part::text("[silent]"), part::from(me)});
  }
  return relay::undefined;
}
// Battle.skillSwap (battle.ts:1325) in Gen 3: the abilities are not named
// and not started.
ADV_HANDLER(skillswap_onHit) {
  ADV_UNUSED;
  Pokemon& user = c.b.at(source);
  Pokemon& other = c.b.at(t.pokemon);
  if (user.fainted || other.fainted) return relay::no;
  const Ability mine = user.ability, yours = other.ability;
  if (mine == Ability::WonderGuard || yours == Ability::WonderGuard || mine == yours) return relay::no;
  // Nothing listens for SetAbility in Gen 3.
  add(c, {part::text("-activate"), part::pokemon(c.b, source), part::text("Skill Swap"), part::text(""), part::text(""),
          part::of(c.b, t.pokemon)});
  single_event(c, callback("", "End"), effect(mine), &user.ability_state, on(source));
  single_event(c, callback("", "End"), effect(yours), &other.ability_state, on(t.pokemon));
  user.ability = yours;
  other.ability = mine;
  refresh_mask(c.b, user);
  refresh_mask(c.b, other);
  user.ability_state = init_effect_state(c.b, yours != Ability::None, source);
  other.ability_state = init_effect_state(c.b, mine != Ability::None, t.pokemon);
  user.volatile_staleness = false;
  if (source.side != t.pokemon.side) other.volatile_staleness = true;
  return relay::undefined;
}
ADV_HANDLER(roleplay_onTryHit) {
  ADV_UNUSED;
  const Ability theirs = c.b.at(t.pokemon).ability;
  return theirs == c.b.at(source).ability || theirs == Ability::WonderGuard ? relay::no : relay::undefined;
}
ADV_HANDLER(roleplay_onHit) {
  ADV_UNUSED;
  return set_ability(c, source, c.b.at(t.pokemon).ability, t.pokemon, effect(Move::RolePlay)) ? relay::undefined : relay::no;
}
ADV_HANDLER(transform_onHit) {
  ADV_UNUSED;
  return transform_into(c, source, t.pokemon) ? relay::yes : relay::no;
}
// Mimic takes the target's last move until its user leaves; Sketch keeps it.
inline Relay learn_last_move(Ctx& c, Ref target, Ref user, Move with, MoveFlag refused) {
  Pokemon& p = c.b.at(user);
  const Pokemon& other = c.b.at(target);
  const Move last = other.last_move;
  if (p.transformed || last == Move::None || volatile_of(c.b.at(target), Condition::Substitute)) return relay::no;
  if (data(last).flags & flag(refused)) return relay::no;
  int index = -1;
  for (int i = 0; i < p.moves; i++) {
    if (same_move(p.move_slots[i].id, last)) return relay::no;
    if (index < 0 && p.move_slots[i].id == with) index = i;
  }
  if (index < 0) return relay::no;
  const Move learned = data(last).real_move != Move::None ? data(last).real_move : last;
  const MoveData& m = data(learned);
  // calculatePP with the PP Ups of the slot: three for Mimic's, none for
  // Sketch's.
  if (with == Move::Mimic) {
    make_virtual(p, index);
    p.move_slots[size_t(index)] = {learned, uint8_t(std::min<int>(5, m.pp)), uint8_t(!m.no_pp_boosts && (p.pp_ups >> index & 1) ? m.pp * 8 / 5 : m.pp), Disabled::No, false, true};
  } else {
    p.move_slots[size_t(index)] = {learned, m.pp, m.pp, Disabled::No, false, true};
  }
  add(c, {part::text("-activate"), part::pokemon(c.b, user), part::text("move: Mimic"), part::move(learned)});
  return relay::undefined;
}
ADV_HANDLER(mimic_onHit) {
  ADV_UNUSED;
  return learn_last_move(c, t.pokemon, source, Move::Mimic, MoveFlag::Failmimic);
}
ADV_HANDLER(sketch_onHit) {
  ADV_UNUSED;
  return learn_last_move(c, t.pokemon, source, Move::Sketch, MoveFlag::Nosketch);
}
// Secret Power's onModifyMove is for terrains, which Gen 3 has none of.
ADV_HANDLER(secretpower_onModifyMove) {
  ADV_UNUSED;
  return relay::undefined;
}
// Weather Ball takes the weather's type, is special in all but a sandstorm,
// and doubles in any weather. Gen 3 never runs its onModifyType.
inline void weather_ball_type(Ctx& c, bool and_category) {
  Battle::ActiveMove& move = c.b.active_move;
  switch (effective_weather(c.b)) {
  case Condition::Sunnyday: move.type = Type::Fire; break;
  case Condition::Raindance: move.type = Type::Water; break;
  case Condition::Sandstorm: move.type = Type::Rock; return;
  case Condition::Hail: move.type = Type::Ice; break;
  default: return;
  }
  if (and_category) move.category = Category::Special;
}
ADV_HANDLER(weatherball_onModifyType) {
  ADV_UNUSED;
  weather_ball_type(c, false);
  return relay::undefined;
}
ADV_HANDLER(weatherball_onModifyMove) {
  ADV_UNUSED;
  weather_ball_type(c, true);
  if (effective_weather(c.b) != Condition::None) c.b.active_move.base_power = uint8_t(c.b.active_move.base_power * 2);
  return relay::undefined;
}
// Follow Me needs more than one Pokémon a side.
ADV_HANDLER(followme_onTry) {
  ADV_UNUSED;
  return relay::no;
}
// BattleActions.useMove(move, pokemon) from a handler: the move is aimed
// anew, at the user or at the foe, and used from the handler's effect.
inline bool call_move(Ctx& c, Move id, Ref user, Effect by) {
  if (const char* why = unsupported(id)) unported(std::string(why) + ", called by another");
  const Target aim = data(id).target;
  const bool on_self = aim == Target::Self || aim == Target::All || aim == Target::AllySide || aim == Target::AllyTeam;
  const Ref target = on_self ? user : aim == Target::AdjacentAlly ? Ref{} : Ref{uint8_t(user.side ^ 1), c.b.sides[user.side ^ 1].active};
  return use_move(c, id, user, target, by);
}
ADV_HANDLER(naturepower_onHit) {
  ADV_UNUSED;
  call_move(c, Move::Swift, t.pokemon, effect(Move::NaturePower));
  return relay::undefined;
}
ADV_HANDLER(metronome_onHit) {
  ADV_UNUSED;
  const MetronomePool& pool = metronome_pool();
  if (!pool.n) return relay::no;
  call_move(c, pool.moves[random(c.b, c.out, uint32_t(pool.n))], t.pokemon, effect(Move::Metronome));
  return relay::undefined;
}
// Assist: a move of another Pokémon of the side, fainted or not.
ADV_HANDLER(assist_onHit) {
  ADV_UNUSED;
  const Side& side = c.b.sides[t.pokemon.side];
  std::array<Move, TEAM_SIZE * MOVE_SLOTS> moves{};
  uint32_t n = 0;
  for (int position = 0; position < side.size; position++) {
    for (int i = 0; i < side.size; i++) {
      const Pokemon& p = side.pokemon[size_t(i)];
      if (p.position != position || i == t.pokemon.index) continue;
      for (int m = 0; m < p.moves; m++) {
        if (!(data(p.move_slots[size_t(m)].id).flags & flag(MoveFlag::Noassist))) moves[n++] = p.move_slots[size_t(m)].id;
      }
    }
  }
  if (!n) return relay::no;
  call_move(c, moves[random(c.b, c.out, n)], t.pokemon, effect(Move::Assist));
  return relay::undefined;
}
// Mirror Move: the move its user was last hit by, if the one that used it
// knows it.
ADV_HANDLER(mirrormove_onHit) {
  ADV_UNUSED;
  int by = 0;
  const Pokemon::AttackedBy* last = last_attacked_by(c.b.at(t.pokemon), by);
  if (!last) return relay::no;
  const Pokemon::AttackedBy& hit = *last;
  const Ref hit_source{uint8_t(t.pokemon.side ^ 1), uint8_t(by)};
  if (c.b.at(hit_source).last_move == Move::None || hit.move == Move::None) return relay::no;
  const Move move = data(hit.move).real_move != Move::None ? data(hit.move).real_move : hit.move;
  static constexpr Move NO_MIRROR[] = {Move::Assist,      Move::Curse,     Move::DoomDesire, Move::FocusPunch, Move::FutureSight,
                                       Move::MagicCoat,   Move::Metronome, Move::Mimic,      Move::MirrorMove, Move::NaturePower,
                                       Move::PsychUp,     Move::RolePlay,  Move::Sketch,     Move::SleepTalk,  Move::Spikes,
                                       Move::SpitUp,      Move::Taunt,     Move::TeeterDance, Move::Transform};
  for (const Move no : NO_MIRROR) {
    if (no == move) return relay::no;
  }
  const Pokemon& attacker = c.b.at(hit_source);
  bool known = false;
  for (int i = 0; i < attacker.moves; i++) known |= same_move(attacker.move_slots[size_t(i)].id, move);
  if (!known) return relay::no;
  call_move(c, move, t.pokemon, effect(Move::MirrorMove));
  return relay::undefined;
}
// Magic Coat sends a move that can be reflected back at its user.
ADV_HANDLER(magiccoat_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singleturn"), part::pokemon(c.b, t.pokemon), part::text("move: Magic Coat")});
  return relay::undefined;
}
ADV_HANDLER(magiccoat_condition_onTryHit) {
  ADV_UNUSED;
  const Move id = c.b.active_move.id;
  if (t.pokemon == source || c.b.active_move.has_bounced || !(data(id).flags & flag(MoveFlag::Reflectable))) return relay::undefined;
  remove_volatile(c, t.pokemon, Condition::MagicCoat);
  use_move(c, id, t.pokemon, source, effect(Condition::MagicCoat), true);
  return relay::null;
}
// Snatch takes a move its target would have used on itself.
ADV_HANDLER(snatch_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singleturn"), part::pokemon(c.b, t.pokemon), part::text("Snatch")});
  return relay::undefined;
}
ADV_HANDLER(snatch_condition_onAnyPrepareHit) {
  ADV_UNUSED;
  const Ref snatcher = c.effect_state->source;
  const Move id = c.b.active_move.id;
  if (!(data(id).flags & flag(MoveFlag::Snatch))) return relay::undefined;
  remove_volatile(c, snatcher, Condition::Snatch);
  add(c, {part::text("-activate"), part::pokemon(c.b, snatcher), part::text("move: Snatch"), part::of(c.b, t.pokemon)});
  // The Pokémon whose move it is makes Snatch cost more, if it has Pressure.
  const Relay drop = run_event(c, events::DEDUCT_PP, on(t.pokemon), snatcher, effect(Move::Snatch));
  if (drop.kind == Relay::Kind::Number && drop.number > 0) deduct_pp(c.b.at(snatcher), Move::Snatch, int(drop.number));
  call_move(c, id, snatcher, effect(Condition::Snatch));
  return relay::null;
}
// Future Sight and Doom Desire: the damage is worked out now, as a typeless
// hit that cannot be critical, and lands two turns later on whatever stands
// in the target's place.
inline Relay future_move(Ctx& c, Ref user, Ref target, Move id, uint8_t power, Category category) {
  if (!add_slot_condition(c, target.side, Condition::Futuremove, user, effect(id))) return relay::no;
  const Battle::ActiveMove real = c.b.active_move;
  c.b.active_move = {.id = id, .type = Type::Unknown, .category = category, .base_power = power, .never_crit = true};
  const Relay damage = get_damage(c, user, target, false);
  c.b.active_move = real;
  if (damage.kind != Relay::Kind::Number) unported("a future move with no damage");
  SideCondition* future = slot_condition_of(c.b.sides[target.side], Condition::Futuremove);
  future->state.duration = 3;
  future->state.move = id;
  future->state.source = user;
  future->state.damage = uint16_t(damage.number);
  add(c, {part::text("-start"), part::pokemon(c.b, user), part::move(id)});
  return relay::null;
}
ADV_HANDLER(futuresight_onTry) {
  ADV_UNUSED;
  return future_move(c, t.pokemon, source, Move::FutureSight, 80, Category::Special);
}
ADV_HANDLER(doomdesire_onTry) {
  ADV_UNUSED;
  return future_move(c, t.pokemon, source, Move::DoomDesire, 120, Category::Physical);
}
// `counter` is Showdown's endingTurn.
ADV_HANDLER(futuremove_onStart) {
  ADV_UNUSED;
  c.effect_state->counter = uint16_t(c.b.turn + 1);
  return relay::undefined;
}
ADV_HANDLER(futuremove_onRestart) {
  ADV_UNUSED;
  return relay::no;
}
ADV_HANDLER(futuremove_onResidual) {
  ADV_UNUSED;
  if (int(c.b.turn) - 1 < int(c.effect_state->counter)) return relay::undefined;
  remove_slot_condition(c, t.pokemon.side, Condition::Futuremove);
  return relay::undefined;
}
ADV_HANDLER(futuremove_onEnd) {
  ADV_UNUSED;
  const EffectState data_ = *c.effect_state;
  const bool sight = data_.move == Move::FutureSight;
  const Pokemon& p = c.b.at(t.pokemon);
  if (p.fainted || t.pokemon == data_.source) {
    add(c, {part::text("-hint"), part::text(p.fainted ? (sight ? "Future Sight did not hit because the target is fainted."
                                                                 : "Doom Desire did not hit because the target is fainted.")
                                                      : (sight ? "Future Sight did not hit because the target is the user."
                                                               : "Doom Desire did not hit because the target is the user."))});
    return relay::undefined;
  }
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::fullname(effect(data_.move))});
  c.b.active_move = {.id = data_.move, .type = Type::Unknown, .category = sight ? Category::Special : Category::Physical,
                     .accuracy = uint8_t(sight ? 90 : 85), .target = Target::Normal, .fixed_damage = data_.damage};
  try_spread_move_hit(c, t.pokemon, data_.source);
  c.b.has_active_move = false;
  check_win(c);
  return relay::undefined;
}
ADV_HANDLER(batonpass_onHit) {
  ADV_UNUSED;
  if (!can_switch(c.b, t.pokemon.side)) {
    attr_last_move(c, "[still]");
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon)});
    return relay::null; // NOT_FAIL
  }
  return relay::undefined;
}

// Substitute.
ADV_HANDLER(substitute_onTryHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (volatile_of(c.b.at(t.pokemon), Condition::Substitute)) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("move: Substitute")});
    return relay::null; // NOT_FAIL: failed, and said so
  }
  if (double(p.hp) <= double(p.maxhp) / 4 || p.maxhp == 1) {
    add(c, {part::text("-fail"), part::pokemon(c.b, t.pokemon), part::text("move: Substitute"), part::text("[weak]")});
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(substitute_onHit) {
  ADV_UNUSED;
  direct_damage(c, double(c.b.at(t.pokemon).maxhp) / 4);
  return relay::undefined;
}
ADV_HANDLER(substitute_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("Substitute")});
  c.effect_state->hp = uint16_t(c.b.at(t.pokemon).maxhp / 4);
  // A binding move lets go: the volatile is deleted, with nothing run.
  if (Volatile* bound = volatile_of(c.b.at(t.pokemon), Condition::Partiallytrapped)) {
    add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::name(bound->state.source_effect), part::text("[partiallytrapped]"),
            part::text("[silent]")});
    bound->id = Condition::None;
  }
  return relay::undefined;
}
ADV_HANDLER(substitute_condition_onEnd) {
  ADV_UNUSED;
  add(c, {part::text("-end"), part::pokemon(c.b, t.pokemon), part::text("Substitute")});
  return relay::undefined;
}
// onTryPrimaryHit(target, source, move): the substitute takes the hit.
ADV_HANDLER(substitute_condition_onTryPrimaryHit) {
  ADV_UNUSED;
  const Move id = c.b.active_move.id;
  if (t.pokemon == source || (data(id).flags & flag(MoveFlag::Bypasssub))) return relay::undefined;
  const Relay calculated = get_damage(c, source, t.pokemon);
  if (calculated.kind != Relay::Kind::Number) {
    add(c, {part::text("-fail"), part::pokemon(c.b, source)});
    attr_last_move(c, "[still]");
    return relay::null;
  }
  Volatile* sub = volatile_of(c.b.at(t.pokemon), Condition::Substitute);
  const uint16_t dealt = uint16_t(std::min<double>(calculated.number, sub->state.hp));
  sub->state.hp = uint16_t(sub->state.hp - dealt);
  c.b.at(source).last_damage = dealt;
  if (!sub->state.hp) {
    remove_volatile(c, t.pokemon, Condition::Substitute);
    add_volatile(c, t.pokemon, Condition::Substitutebroken);
    if (Volatile* broken = volatile_of(c.b.at(t.pokemon), Condition::Substitutebroken)) broken->state.move = id;
  } else {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("Substitute"), part::text("[damage]")});
  }
  if (data(id).ohko) add(c, {part::text("-ohko")});
  if (dealt) apply_recoil_damage(c, dealt, source);
  if (const Fraction drain = data(id).drain) {
    heal(c, std::ceil(double(dealt) * drain.num / drain.den), source, t.pokemon, effect(Special::Drain));
  }
  // AfterSubDamage is run on the target, where nothing listens.
  return relay::number(0); // HIT_SUBSTITUTE
}
// Protect, Detect and Endure work if something is still to act, and less
// often each time in a row.
ADV_HANDLER(stalling_onPrepareHit) {
  ADV_UNUSED;
  if (!will_act(c.b)) return relay::no;
  return run_event(c, event("StallMove"), on(t.pokemon)).truthy() ? relay::yes : relay::no;
}
ADV_HANDLER(stalling_onHit) {
  ADV_UNUSED;
  add_volatile(c, t.pokemon, Condition::Stall);
  return relay::undefined;
}
ADV_HANDLER(stall_onStart) {
  ADV_UNUSED;
  c.effect_state->counter = 2;
  return relay::undefined;
}
ADV_HANDLER(stall_onStallMove) {
  ADV_UNUSED;
  const uint16_t counter = c.effect_state->counter ? c.effect_state->counter : 1;
  if (counter >= 256) unported("a stalling move used nine times in a row");
  return random(c.b, c.out, counter) < 1 ? relay::yes : relay::no; // randomChance(1, counter)
}
ADV_HANDLER(stall_onRestart) {
  ADV_UNUSED;
  if (c.effect_state->counter < data(Condition::Stall).counter_max) c.effect_state->counter = uint16_t(c.effect_state->counter * 2);
  c.effect_state->duration = 2;
  return relay::undefined;
}
ADV_HANDLER(protect_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singleturn"), part::pokemon(c.b, t.pokemon), part::text("Protect")});
  return relay::undefined;
}
ADV_HANDLER(protect_condition_onTryHit) {
  ADV_UNUSED;
  // checkMoveBypassesProtect: a move without the protect flag goes through.
  if (!(data(c.b.active_move.id).flags & flag(MoveFlag::Protect))) return relay::undefined;
  add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("Protect")});
  // The count of an Outrage is not reset, but it is kept going.
  if (Volatile* locked = volatile_of(c.b.at(source), Condition::Lockedmove); locked && locked->state.time >= 2) locked->state.duration = 2;
  return relay::null;
}
ADV_HANDLER(endure_condition_onStart) {
  ADV_UNUSED;
  add(c, {part::text("-singleturn"), part::pokemon(c.b, t.pokemon), part::text("move: Endure")});
  return relay::undefined;
}
ADV_HANDLER(endure_condition_onDamage) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (source_effect.kind == EffectKind::Move && source_effect && !(data(Move(source_effect.id)).flags & flag(MoveFlag::Futuremove)) &&
      value.number >= p.hp) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Endure")});
    return relay::number(p.hp - 1);
  }
  return relay::undefined;
}

// Side conditions (their moves' `condition`). `t` is the side.
ADV_HANDLER(spikes_onSideStart) {
  ADV_UNUSED;
  add(c, {part::text("-sidestart"), part::side_name(t.side), part::text("Spikes")});
  c.effect_state->layers = 1;
  return relay::undefined;
}
ADV_HANDLER(spikes_onSideRestart) {
  ADV_UNUSED;
  if (c.effect_state->layers >= 3) return relay::no;
  add(c, {part::text("-sidestart"), part::side_name(t.side), part::text("Spikes")});
  c.effect_state->layers++;
  return relay::undefined;
}
ADV_HANDLER(spikes_onEntryHazard) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (!is_grounded(p)) return relay::undefined;
  static constexpr int EIGHTHS_OF_THREE[] = {0, 3, 4, 6};
  damage(c, double(EIGHTHS_OF_THREE[c.effect_state->layers]) * p.maxhp / 24);
  return relay::undefined;
}
// The screens last five turns and halve what a hit of their kind does,
// unless it is a critical hit.
#define ADV_SCREEN(id, shown, category_)                                                               \
  ADV_HANDLER(id##_durationCallback) {                                                                 \
    ADV_UNUSED;                                                                                        \
    return relay::number(5);                                                                           \
  }                                                                                                    \
  ADV_HANDLER(id##_onSideStart) {                                                                      \
    ADV_UNUSED;                                                                                        \
    add(c, {part::text("-sidestart"), part::side_name(t.side), part::text(shown)});                    \
    return relay::undefined;                                                                           \
  }                                                                                                    \
  ADV_HANDLER(id##_onSideEnd) {                                                                        \
    ADV_UNUSED;                                                                                        \
    add(c, {part::text("-sideend"), part::side_name(t.side), part::text(shown)});                      \
    return relay::undefined;                                                                           \
  }                                                                                                    \
  ADV_HANDLER(id##_onAnyModifyDamagePhase1) {                                                          \
    ADV_UNUSED;                                                                                        \
    /* (damage, source, target, move): `t` attacks and `source` is hit. */                            \
    if (source != t.pokemon && c.effect_state->target_side == source.side &&                           \
        c.b.active_move.category == Category::category_ && !c.b.active_move.crit) {                    \
      chain_modify(c, 0.5);                                                                            \
    }                                                                                                  \
    return relay::undefined;                                                                           \
  }
ADV_SCREEN(reflect, "Reflect", Physical)
ADV_SCREEN(lightscreen, "move: Light Screen", Special)
#undef ADV_SCREEN
inline bool has_secondary(Effect move) { return move.kind == EffectKind::Move && data(Move(move.id)).secondary.chance; }
ADV_HANDLER(safeguard_durationCallback) {
  ADV_UNUSED;
  return relay::number(5);
}
ADV_HANDLER(safeguard_onSideStart) {
  ADV_UNUSED;
  add(c, {part::text("-sidestart"), part::side_name(t.side), part::text("Safeguard")});
  return relay::undefined;
}
ADV_HANDLER(safeguard_onSideEnd) {
  ADV_UNUSED;
  add(c, {part::text("-sideend"), part::side_name(t.side), part::text("Safeguard")});
  return relay::undefined;
}
ADV_HANDLER(safeguard_onSetStatus) {
  ADV_UNUSED;
  if (!source_effect || !source) return relay::undefined;
  // Sleep from a Yawn already under way is let through.
  if (source_effect == effect(Condition::Yawn) || source_effect == effect(Move::Yawn)) return relay::undefined;
  if (t.pokemon == source) return relay::undefined;
  if (source_effect == effect(Ability::Synchronize) || (source_effect.kind == EffectKind::Move && !has_secondary(source_effect))) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Safeguard")});
  }
  return relay::null;
}
ADV_HANDLER(safeguard_onTryAddVolatile) {
  ADV_UNUSED;
  if (!source_effect || !source) return relay::undefined;
  if ((value.text == "confusion" || value.text == "yawn") && t.pokemon != source) {
    if (source_effect.kind == EffectKind::Move && !has_secondary(source_effect)) {
      add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Safeguard")});
    }
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(mist_onSideStart) {
  ADV_UNUSED;
  add(c, {part::text("-sidestart"), part::side_name(t.side), part::text("Mist")});
  return relay::undefined;
}
ADV_HANDLER(mist_onSideEnd) {
  ADV_UNUSED;
  add(c, {part::text("-sideend"), part::side_name(t.side), part::text("Mist")});
  return relay::undefined;
}
ADV_HANDLER(mist_onTryBoost) {
  ADV_UNUSED;
  if (!source || t.pokemon == source) return relay::undefined;
  bool show = false;
  for (int stat = 0; stat < BOOST_STATS; stat++) {
    if (value.boosts.has(stat) && value.boosts.value[size_t(stat)] < 0) value.boosts.remove(stat), show = true;
  }
  if (show && !has_secondary(source_effect)) {
    add(c, {part::text("-activate"), part::pokemon(c.b, t.pokemon), part::text("move: Mist")});
  }
  return value;
}
ADV_HANDLER(brickbreak_onTryHit) {
  ADV_UNUSED;
  remove_side_condition(c, source.side ^ 1, Condition::Reflect);
  remove_side_condition(c, source.side ^ 1, Condition::LightScreen);
  return relay::undefined;
}
// Rapid Spin frees its user: of Leech Seed, of Spikes on its side, of a
// binding move. Its onAfterHit and its `self.onHit` both do so.
inline void rapid_spin(Ctx& c, Ref pokemon) {
  if (remove_volatile(c, pokemon, Condition::LeechSeed)) {
    add(c, {part::text("-end"), part::pokemon(c.b, pokemon), part::text("Leech Seed"), part::from(effect(Move::RapidSpin)),
            part::of(c.b, pokemon)});
  }
  if (remove_side_condition(c, pokemon.side, Condition::Spikes)) {
    add(c, {part::text("-sideend"), part::side_name(pokemon.side), part::text("Spikes"), part::from(effect(Move::RapidSpin)),
            part::of(c.b, pokemon)});
  }
  // The binding move's own onEnd writes its line.
  remove_volatile(c, pokemon, Condition::Partiallytrapped);
}
ADV_HANDLER(rapidspin_onAfterHit) {
  ADV_UNUSED;
  rapid_spin(c, source);
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
  if (data(Move(source_effect.id)).ohko) {
    add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::Sturdy))});
    return relay::null;
  }
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
  for (const Type type : p.types) {
    if (type == Type::None) continue;
    const uint8_t taken = damage_taken(type, move.type);
    total += taken == 1 ? 1 : taken == 2 ? -1 : 0;
  }
  // runImmunity, which Foresight can turn off for a Ghost.
  if (total <= 0 || !run_immunity(c, t.pokemon, false)) {
    add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::from(effect(Ability::WonderGuard))});
    return relay::null;
  }
  return relay::undefined;
}
ADV_HANDLER(flashfire_onTryHit) {
  ADV_UNUSED;
  if (t.pokemon == source || move_type(c, source_effect) != Type::Fire) return relay::undefined;
  if (Move(source_effect.id) == Move::WillOWisp) {
    Pokemon& p = c.b.at(t.pokemon);
    if (has_type(p, Type::Fire) || p.status != Condition::None || volatile_of(p, Condition::Substitute)) return relay::undefined;
  }
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
// A draw on every contact, and on one in three a status for the attacker.
#define ADV_CONTACT_STATUS(name, status)                                                \
  ADV_HANDLER(name) {                                                                   \
    ADV_UNUSED;                                                                         \
    if (made_contact(value, source_effect) && random(c.b, c.out, 3) < 1) {              \
      try_set_status(c, source, Condition::status, t.pokemon);                          \
    }                                                                                   \
    return relay::undefined;                                                            \
  }
ADV_CONTACT_STATUS(static_onDamagingHit, Par)
ADV_CONTACT_STATUS(flamebody_onDamagingHit, Brn)
ADV_CONTACT_STATUS(poisonpoint_onDamagingHit, Psn)
#undef ADV_CONTACT_STATUS
ADV_HANDLER(cutecharm_onDamagingHit) {
  ADV_UNUSED;
  if (made_contact(value, source_effect) && random(c.b, c.out, 3) < 1) add_volatile(c, source, Condition::Attract, t.pokemon);
  return relay::undefined;
}
ADV_HANDLER(effectspore_onDamagingHit) {
  ADV_UNUSED;
  if (made_contact(value, source_effect) && random(c.b, c.out, 10) < 1) {
    static constexpr Condition STATUS[] = {Condition::Slp, Condition::Par, Condition::Psn};
    try_set_status(c, source, STATUS[random(c.b, c.out, 3)], t.pokemon);
  }
  return relay::undefined;
}
ADV_HANDLER(colorchange_onDamagingHit) {
  ADV_UNUSED;
  const Pokemon& p = c.b.at(t.pokemon);
  if (!value.number || !p.hp) return relay::undefined;
  const Battle::ActiveMove& move = c.b.active_move;
  if (p.is_active && move.category != Category::Status && move.type != Type::Unknown && !has_type(p, move.type)) {
    // Pokemon.setType.
    c.b.at(t.pokemon).types = {move.type, Type::None};
    c.b.at(t.pokemon).known_type = true;
    add(c, {part::text("-start"), part::pokemon(c.b, t.pokemon), part::text("typechange"), part::text(TYPE_NAMES[uint8_t(move.type)].name),
            part::from(effect(Ability::ColorChange))});
  }
  return relay::undefined;
}
ADV_HANDLER(serenegrace_onModifyMove) {
  ADV_UNUSED;
  c.b.active_move.secondary_chance = uint8_t(std::min(c.b.active_move.secondary_chance * 2, 255));
  c.b.active_move.flinch_chance = uint8_t(c.b.active_move.flinch_chance * 2);
  return relay::undefined;
}
ADV_HANDLER(damp_onAnyTryMove) {
  ADV_UNUSED;
  const Move move = Move(source_effect.id);
  if (move == Move::Explosion || move == Move::SelfDestruct) {
    attr_last_move(c, "[still]");
    add(c, {part::text("cant"), part::pokemon(c.b, c.effect_state->target_pokemon), part::text("ability: Damp"), part::move(move),
            part::of(c.b, t.pokemon)});
    return relay::no;
  }
  return relay::undefined;
}
ADV_HANDLER(owntempo_onHit) {
  ADV_UNUSED;
  if (source_effect.kind == EffectKind::Move && data(Move(source_effect.id)).volatile_status == Condition::Confusion) {
    add(c, {part::text("-immune"), part::pokemon(c.b, t.pokemon), part::text("confusion"), part::from(effect(Ability::OwnTempo))});
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
    A(Truant, onResidual, truant_onResidual),
    A(Truant, onBeforeMove, truant_onBeforeMove),
    A(SandStream, onStart, sandstream_onStart),
    A(SandStream, onAfterEntryHazard, sandstream_onAfterEntryHazard),
    A(Drizzle, onStart, drizzle_onStart),
    A(Drizzle, onAfterEntryHazard, drizzle_onStart),
    A(Drought, onStart, drought_onStart),
    A(Drought, onAfterEntryHazard, drought_onStart),
    A(Forecast, onStart, forecast_onStart),
    A(Forecast, onAfterEntryHazard, forecast_onStart),
    A(Forecast, onWeatherChange, forecast_onWeatherChange),
    A(RainDish, onResidual, raindish_onResidual),
    A(Damp, onAnyDamage, damp_onAnyDamage),
    I(WhiteHerb, onStart, whiteherb_onStart),
    I(WhiteHerb, onAnySwitchIn, whiteherb_onStart),
    I(WhiteHerb, onAnyAfterMega, whiteherb_onStart),
    I(WhiteHerb, onAnyAfterMove, whiteherb_onStart),
    I(WhiteHerb, onResidual, whiteherb_onStart),
    I(WhiteHerb, onUse, whiteherb_onUse),
    I(KingsRock, onModifyMove, kingsrock_onModifyMove),
    I(LansatBerry, onEat, lansatberry_onEat),
    I(Mail, onTakeItem, mail_onTakeItem),
    C(FocusEnergy, onStart, focusenergy_condition_onStart),
    C(FocusEnergy, onModifyCritRatio, focusenergy_condition_onModifyCritRatio),
    C(Charge, onStart, charge_condition_onStart),
    C(Charge, onRestart, charge_condition_onStart),
    C(Charge, onBasePower, charge_condition_onBasePower),
    C(Charge, onMoveAborted, charge_condition_onAfterMove),
    C(Charge, onAfterMove, charge_condition_onAfterMove),
    C(Charge, onEnd, charge_condition_onEnd),
    C(Partiallytrapped, onStart, partiallytrapped_onStart),
    C(Partiallytrapped, onResidual, partiallytrapped_onResidual),
    C(Partiallytrapped, onEnd, partiallytrapped_onEnd),
    C(Partiallytrapped, onTrapPokemon, partiallytrapped_onTrapPokemon),
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
    I(AguavBerry, onTryEatItem, aguavberry_onTryEatItem),
    I(FigyBerry, onTryEatItem, figyberry_onTryEatItem),
    I(IapapaBerry, onTryEatItem, iapapaberry_onTryEatItem),
    I(MagoBerry, onTryEatItem, magoberry_onTryEatItem),
    I(WikiBerry, onTryEatItem, wikiberry_onTryEatItem),
    I(OranBerry, onTryEatItem, oranberry_onTryEatItem),
    I(SitrusBerry, onTryEatItem, sitrusberry_onTryEatItem),
    I(AguavBerry, onEat, aguavberry_onEat),
    I(FigyBerry, onEat, figyberry_onEat),
    I(IapapaBerry, onEat, iapapaberry_onEat),
    I(MagoBerry, onEat, magoberry_onEat),
    I(WikiBerry, onEat, wikiberry_onEat),
    I(OranBerry, onEat, oranberry_onEat),
    I(SitrusBerry, onEat, sitrusberry_onEat),
    I(ApicotBerry, onEat, apicotberry_onEat),
    I(GanlonBerry, onEat, ganlonberry_onEat),
    I(LiechiBerry, onEat, liechiberry_onEat),
    I(PetayaBerry, onEat, petayaberry_onEat),
    I(SalacBerry, onEat, salacberry_onEat),
    I(StarfBerry, onEat, starfberry_onEat),
    I(AspearBerry, onEat, aspearberry_onEat),
    I(CheriBerry, onEat, cheriberry_onEat),
    I(ChestoBerry, onEat, chestoberry_onEat),
    I(PechaBerry, onEat, pechaberry_onEat),
    I(RawstBerry, onEat, rawstberry_onEat),
    I(LumBerry, onEat, lumberry_onEat),
    I(PersimBerry, onEat, persimberry_onEat),
    I(LeppaBerry, onEat, leppaberry_onEat),
    I(LumBerry, onAfterSetStatus, lumberry_onAfterSetStatus),
    C(Confusion, onStart, confusion_onStart),
    C(Confusion, onEnd, confusion_onEnd),
    C(Confusion, onBeforeMove, confusion_onBeforeMove),
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
    {effect(Move::FocusPunch), Callback::beforeTurnCallback, &focuspunch_beforeTurnCallback},
    {effect(Move::FocusPunch), Callback::onTry, &focuspunch_onTry},
    C(FocusPunch, onStart, focuspunch_condition_onStart),
    C(FocusPunch, onHit, focuspunch_condition_onHit),
    C(FocusPunch, onTryAddVolatile, focuspunch_condition_onTryAddVolatile),
    A(SuctionCups, onDragOut, suctioncups_onDragOut),
    {effect(Move::LeechSeed), Callback::onTryImmunity, &leechseed_onTryImmunity},
    C(LeechSeed, onStart, leechseed_condition_onStart),
    C(LeechSeed, onResidual, leechseed_condition_onResidual),
    C(Taunt, onStart, taunt_condition_onStart),
    C(Taunt, onEnd, taunt_condition_onEnd),
    C(Taunt, onDisableMove, taunt_condition_onDisableMove),
    C(Taunt, onBeforeMove, taunt_condition_onBeforeMove),
    C(Wish, onEnd, wish_condition_onEnd),
    {effect(Move::KnockOff), Callback::onAfterHit, &knockoff_onAfterHit},
    {effect(Move::Aromatherapy), Callback::onHit, &aromatherapy_onHit},
    {effect(Move::HealBell), Callback::onHit, &healbell_onHit},
    {effect(Move::Haze), Callback::onHitField, &haze_onHitField},
    {effect(Move::Pursuit), Callback::beforeTurnCallback, &pursuit_beforeTurnCallback},
    {effect(Move::Pursuit), Callback::basePowerCallback, &pursuit_basePowerCallback},
    {effect(Move::Pursuit), Callback::onModifyMove, &pursuit_onModifyMove},
    C(Pursuit, onFoeBeforeSwitchOut, pursuit_condition_onFoeBeforeSwitchOut),
    {effect(Move::Counter), Callback::beforeTurnCallback, &counter_beforeTurnCallback},
    {effect(Move::Counter), Callback::onTry, &counter_onTry},
    {effect(Move::Counter), Callback::damageCallback, &counter_damageCallback},
    C(Counter, onStart, counter_condition_onStart),
    C(Counter, onDamage, counter_condition_onDamage),
    {effect(Move::MirrorCoat), Callback::beforeTurnCallback, &mirrorcoat_beforeTurnCallback},
    {effect(Move::MirrorCoat), Callback::onTry, &mirrorcoat_onTry},
    {effect(Move::MirrorCoat), Callback::damageCallback, &mirrorcoat_damageCallback},
    C(MirrorCoat, onStart, mirrorcoat_condition_onStart),
    C(MirrorCoat, onDamage, mirrorcoat_condition_onDamage),
    {effect(Move::Endeavor), Callback::onTry, &endeavor_onTry},
    {effect(Move::Endeavor), Callback::onTryImmunity, &endeavor_onTryImmunity},
    {effect(Move::Endeavor), Callback::damageCallback, &endeavor_damageCallback},
    {effect(Move::PerishSong), Callback::onHitField, &perishsong_onHitField},
    C(PerishSong, onResidual, perishsong_condition_onResidual),
    C(PerishSong, onEnd, perishsong_condition_onEnd),
    {effect(Move::Thief), Callback::onAfterHit, &thief_onAfterHit},
    {effect(Move::Covet), Callback::onAfterHit, &covet_onAfterHit},
    A(StickyHold, onTakeItem, stickyhold_onTakeItem),
    {effect(Move::PsychUp), Callback::onHit, &psychup_onHit},
    A(CloudNine, onStart, weatherlock_onStart),
    A(CloudNine, onAfterEntryHazard, weatherlock_onStart),
    A(CloudNine, onEnd, weatherlock_onEnd),
    A(AirLock, onStart, weatherlock_onStart),
    A(AirLock, onAfterEntryHazard, weatherlock_onStart),
    A(AirLock, onEnd, weatherlock_onEnd),
    A(Trace, onStart, trace_onStart),
    {effect(Move::Reversal), Callback::basePowerCallback, &lowhp_basePowerCallback},
    {effect(Move::Flail), Callback::basePowerCallback, &lowhp_basePowerCallback},
    {effect(Move::Eruption), Callback::basePowerCallback, &fullhp_basePowerCallback},
    {effect(Move::WaterSpout), Callback::basePowerCallback, &fullhp_basePowerCallback},
    {effect(Move::LowKick), Callback::basePowerCallback, &lowkick_basePowerCallback},
    {effect(Move::Facade), Callback::onBasePower, &facade_onBasePower},
    {effect(Move::SmellingSalts), Callback::basePowerCallback, &smellingsalts_basePowerCallback},
    {effect(Move::SmellingSalts), Callback::onHit, &smellingsalts_onHit},
    C(Disable, durationCallback, two_to_five_durationCallback),
    C(Uproar, durationCallback, two_to_five_durationCallback),
    C(Encore, durationCallback, three_to_six_durationCallback),
    C(Partiallytrapped, durationCallback, three_to_six_durationCallback),
    {effect(Move::Struggle), Callback::onModifyMove, &struggle_onModifyMove},
    {effect(Move::SleepTalk), Callback::onTry, &sleeptalk_onTry},
    {effect(Move::SleepTalk), Callback::onTryHit, &sleeptalk_onTryHit},
    {effect(Move::SleepTalk), Callback::onHit, &sleeptalk_onHit},
    {effect(Move::BeatUp), Callback::onModifyMove, &beatup_onModifyMove},
    {effect(Move::BeatUp), Callback::basePowerCallback, &beatup_basePowerCallback},
    C(BeatUp, onModifySpA, beatup_condition_onModifySpA),
    C(BeatUp, onFoeModifySpD, beatup_condition_onFoeModifySpD),
    {effect(Move::MeanLook), Callback::onHit, &trapping_onHit},
    {effect(Move::SpiderWeb), Callback::onHit, &trapping_onHit},
    {effect(Move::Block), Callback::onHit, &trapping_onHit},
    C(Trapped, onStart, trapped_onStart),
    C(Trapped, onTrapPokemon, trapped_onTrapPokemon),
    {effect(Move::DestinyBond), Callback::onPrepareHit, &destinybond_onPrepareHit},
    C(DestinyBond, onStart, destinybond_condition_onStart),
    C(DestinyBond, onFaint, destinybond_condition_onFaint),
    C(DestinyBond, onBeforeMove, destinybond_condition_onBeforeMove),
    C(DestinyBond, onMoveAborted, destinybond_condition_onMoveAborted),
    {effect(Move::SolarBeam), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::SolarBeam), Callback::onBasePower, &solarbeam_onBasePower},
    {effect(Move::Fly), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::Dig), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::Dive), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::Bounce), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::SkullBash), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::RazorWind), Callback::onTryMove, &charge_onTryMove},
    {effect(Move::SkyAttack), Callback::onTryMove, &charge_onTryMove},
    C(Twoturnmove, onStart, twoturnmove_onStart),
    C(Twoturnmove, onEnd, twoturnmove_onEnd),
    C(Twoturnmove, onLockMove, twoturnmove_onLockMove),
    C(Twoturnmove, onMoveAborted, twoturnmove_onMoveAborted),
    C(Fly, onInvulnerability, fly_condition_onInvulnerability),
    C(Fly, onSourceModifyDamage, fly_condition_onSourceModifyDamage),
    C(Bounce, onInvulnerability, fly_condition_onInvulnerability),
    C(Bounce, onSourceBasePower, bounce_condition_onSourceBasePower),
    C(Dig, onInvulnerability, dig_condition_onInvulnerability),
    C(Dig, onSourceModifyDamage, dig_condition_onSourceModifyDamage),
    C(Dig, onImmunity, underground_onImmunity),
    C(Dive, onInvulnerability, dive_condition_onInvulnerability),
    C(Dive, onSourceModifyDamage, dive_condition_onSourceModifyDamage),
    C(Dive, onImmunity, underground_onImmunity),
    C(Lockedmove, onStart, lockedmove_onStart),
    C(Lockedmove, onRestart, lockedmove_onRestart),
    C(Lockedmove, onResidual, lockedmove_onResidual),
    C(Lockedmove, onEnd, lockedmove_onEnd),
    C(Lockedmove, onLockMove, lockedmove_onLockMove),
    C(Mustrecharge, onStart, mustrecharge_onStart),
    C(Mustrecharge, onBeforeMove, mustrecharge_onBeforeMove),
    C(Mustrecharge, onLockMove, mustrecharge_onLockMove),
    {effect(Move::Spite), Callback::onHit, &spite_onHit},
    {effect(Move::Stockpile), Callback::onTry, &stockpile_onTry},
    C(Stockpile, onStart, stockpile_condition_onStart),
    C(Stockpile, onRestart, stockpile_condition_onRestart),
    C(Stockpile, onEnd, stockpile_condition_onEnd),
    {effect(Move::Swallow), Callback::onTry, &needs_stockpile_onTry},
    {effect(Move::Swallow), Callback::onHit, &swallow_onHit},
    {effect(Move::SpitUp), Callback::onTry, &needs_stockpile_onTry},
    {effect(Move::SpitUp), Callback::basePowerCallback, &spitup_basePowerCallback},
    {effect(Move::SpitUp), Callback::onAfterMove, &spitup_onAfterMove},
    {effect(Move::DreamEater), Callback::onTryImmunity, &dreameater_onTryImmunity},
    {effect(Move::Moonlight), Callback::onHit, &weather_heal_onHit},
    {effect(Move::MorningSun), Callback::onHit, &weather_heal_onHit},
    {effect(Move::Synthesis), Callback::onHit, &weather_heal_onHit},
    {effect(Move::PayDay), Callback::onHit, &payday_onHit},
    {effect(Move::Snore), Callback::onTry, &snore_onTry},
    {effect(Move::Psywave), Callback::damageCallback, &psywave_damageCallback},
    {effect(Move::Magnitude), Callback::onModifyMove, &magnitude_onModifyMove},
    {effect(Move::Magnitude), Callback::onUseMoveMessage, &magnitude_onUseMoveMessage},
    {effect(Move::Teleport), Callback::onTry, &teleport_onTry},
    {effect(Move::Revenge), Callback::basePowerCallback, &revenge_basePowerCallback},
    {effect(Move::FalseSwipe), Callback::onDamage, &falseswipe_onDamage},
    {effect(Move::Present), Callback::onModifyMove, &present_onModifyMove},
    {effect(Move::Splash), Callback::onTry, &splash_onTry},
    {effect(Move::Splash), Callback::onTryHit, &splash_onTryHit},
    {effect(Move::SuperFang), Callback::damageCallback, &superfang_damageCallback},
    {effect(Move::Foresight), Callback::onTryHit, &foresight_onTryHit},
    {effect(Move::OdorSleuth), Callback::onTryHit, &foresight_onTryHit},
    C(Foresight, onStart, foresight_condition_onStart),
    C(Foresight, onNegateImmunity, foresight_condition_onNegateImmunity),
    C(Foresight, onModifyBoost, foresight_condition_onModifyBoost),
    {effect(Move::LockOn), Callback::onTryHit, &lockon_onTryHit},
    {effect(Move::LockOn), Callback::onHit, &lockon_onHit},
    {effect(Move::MindReader), Callback::onTryHit, &lockon_onTryHit},
    {effect(Move::MindReader), Callback::onHit, &mindreader_onHit},
    C(LockOn, onSourceInvulnerability, lockon_condition_onSourceInvulnerability),
    C(LockOn, onSourceAccuracy, lockon_condition_onSourceAccuracy),
    C(Nightmare, onStart, nightmare_condition_onStart),
    C(Nightmare, onResidual, nightmare_condition_onResidual),
    {effect(Move::Imprison), Callback::onTryHit, &imprison_onTryHit},
    C(Imprison, onStart, imprison_condition_onStart),
    C(Imprison, onFoeDisableMove, imprison_condition_onFoeDisableMove),
    C(Imprison, onFoeBeforeMove, imprison_condition_onFoeBeforeMove),
    C(Ingrain, onStart, ingrain_condition_onStart),
    C(Ingrain, onResidual, ingrain_condition_onResidual),
    C(Ingrain, onTrapPokemon, ingrain_condition_onTrapPokemon),
    C(Ingrain, onDragOut, ingrain_condition_onDragOut),
    C(Minimize, onRestart, null_onRestart),
    C(Minimize, onSourceModifyDamage, minimize_condition_onSourceModifyDamage),
    C(DefenseCurl, onRestart, null_onRestart),
    C(MudSport, onStart, mudsport_condition_onStart),
    C(MudSport, onAnyBasePower, mudsport_condition_onAnyBasePower),
    C(WaterSport, onStart, watersport_condition_onStart),
    C(WaterSport, onAnyBasePower, watersport_condition_onAnyBasePower),
    C(Torment, onStart, torment_condition_onStart),
    C(Torment, onEnd, torment_condition_onEnd),
    C(Torment, onDisableMove, torment_condition_onDisableMove),
    C(Rage, onStart, rage_condition_onStart),
    C(Rage, onHit, rage_condition_onHit),
    C(Rage, onBeforeMove, rage_condition_onBeforeMove),
    C(Grudge, onStart, grudge_condition_onStart),
    C(Grudge, onFaint, grudge_condition_onFaint),
    C(Grudge, onBeforeMove, grudge_condition_onBeforeMove),
    {effect(Move::Attract), Callback::onTryImmunity, &attract_onTryImmunity},
    C(Attract, onStart, attract_condition_onStart),
    C(Attract, onUpdate, attract_condition_onUpdate),
    C(Attract, onBeforeMove, attract_condition_onBeforeMove),
    C(Attract, onEnd, attract_condition_onEnd),
    {effect(Move::JumpKick), Callback::onMoveFail, &crash_onMoveFail},
    {effect(Move::HighJumpKick), Callback::onMoveFail, &crash_onMoveFail},
    {effect(Move::TripleKick), Callback::basePowerCallback, &triplekick_basePowerCallback},
    {effect(Move::Rollout), Callback::basePowerCallback, &rollout_basePowerCallback},
    {effect(Move::Rollout), Callback::onModifyMove, &rollout_onModifyMove},
    {effect(Move::Rollout), Callback::onAfterMove, &rolling_onAfterMove},
    C(Rollout, onLockMove, rollout_condition_onLockMove),
    C(Rollout, onStart, rolling_condition_onStart),
    C(Rollout, onResidual, rollout_condition_onResidual),
    {effect(Move::IceBall), Callback::basePowerCallback, &iceball_basePowerCallback},
    {effect(Move::IceBall), Callback::onModifyMove, &iceball_onModifyMove},
    {effect(Move::IceBall), Callback::onAfterMove, &rolling_onAfterMove},
    C(IceBall, onLockMove, iceball_condition_onLockMove),
    C(IceBall, onStart, rolling_condition_onStart),
    C(IceBall, onResidual, iceball_condition_onResidual),
    {effect(Move::FuryCutter), Callback::basePowerCallback, &furycutter_basePowerCallback},
    {effect(Move::FuryCutter), Callback::onHit, &furycutter_onHit},
    C(FuryCutter, onStart, furycutter_condition_onStart),
    C(FuryCutter, onRestart, furycutter_condition_onRestart),
    {effect(Move::Uproar), Callback::onTryHit, &uproar_onTryHit},
    C(Uproar, onStart, uproar_condition_onStart),
    C(Uproar, onResidual, uproar_condition_onResidual),
    C(Uproar, onEnd, uproar_condition_onEnd),
    C(Uproar, onLockMove, uproar_condition_onLockMove),
    C(Uproar, onAnySetStatus, uproar_condition_onAnySetStatus),
    {effect(Move::Bide), Callback::beforeMoveCallback, &bide_beforeMoveCallback},
    C(Bide, onLockMove, bide_condition_onLockMove),
    C(Bide, onStart, bide_condition_onStart),
    C(Bide, onDamage, bide_condition_onDamage),
    C(Bide, onBeforeMove, bide_condition_onBeforeMove),
    C(Bide, onMoveAborted, bide_condition_onMoveAborted),
    C(Bide, onEnd, bide_condition_onEnd),
    {effect(Move::Conversion), Callback::onHit, &conversion_onHit},
    {effect(Move::Conversion2), Callback::onHit, &conversion2_onHit},
    {effect(Move::Camouflage), Callback::onHit, &camouflage_onHit},
    {effect(Move::Recycle), Callback::onHit, &recycle_onHit},
    {effect(Move::Trick), Callback::onTryImmunity, &trick_onTryImmunity},
    {effect(Move::Trick), Callback::onHit, &trick_onHit},
    {effect(Move::SkillSwap), Callback::onHit, &skillswap_onHit},
    {effect(Move::RolePlay), Callback::onTryHit, &roleplay_onTryHit},
    {effect(Move::RolePlay), Callback::onHit, &roleplay_onHit},
    {effect(Move::Transform), Callback::onHit, &transform_onHit},
    {effect(Move::Mimic), Callback::onHit, &mimic_onHit},
    {effect(Move::Sketch), Callback::onHit, &sketch_onHit},
    {effect(Move::SecretPower), Callback::onModifyMove, &secretpower_onModifyMove},
    {effect(Move::WeatherBall), Callback::onModifyType, &weatherball_onModifyType},
    {effect(Move::WeatherBall), Callback::onModifyMove, &weatherball_onModifyMove},
    {effect(Move::FollowMe), Callback::onTry, &followme_onTry},
    {effect(Move::NaturePower), Callback::onHit, &naturepower_onHit},
    {effect(Move::Metronome), Callback::onHit, &metronome_onHit},
    {effect(Move::Assist), Callback::onHit, &assist_onHit},
    {effect(Move::MirrorMove), Callback::onHit, &mirrormove_onHit},
    C(MagicCoat, onStart, magiccoat_condition_onStart),
    C(MagicCoat, onTryHit, magiccoat_condition_onTryHit),
    C(Snatch, onStart, snatch_condition_onStart),
    C(Snatch, onAnyPrepareHit, snatch_condition_onAnyPrepareHit),
    {effect(Move::FutureSight), Callback::onTry, &futuresight_onTry},
    {effect(Move::DoomDesire), Callback::onTry, &doomdesire_onTry},
    C(Futuremove, onStart, futuremove_onStart),
    C(Futuremove, onRestart, futuremove_onRestart),
    C(Futuremove, onResidual, futuremove_onResidual),
    C(Futuremove, onEnd, futuremove_onEnd),
    C(Encore, onStart, encore_condition_onStart),
    C(Encore, onOverrideAction, encore_condition_onOverrideAction),
    C(Encore, onResidual, encore_condition_onResidual),
    C(Encore, onEnd, encore_condition_onEnd),
    C(Encore, onDisableMove, encore_condition_onDisableMove),
    {effect(Move::Disable), Callback::onTryHit, &disable_onTryHit},
    C(Disable, onStart, disable_condition_onStart),
    C(Disable, onEnd, disable_condition_onEnd),
    C(Disable, onBeforeMove, disable_condition_onBeforeMove),
    C(Disable, onDisableMove, disable_condition_onDisableMove),
    {effect(Move::Yawn), Callback::onTryHit, &yawn_onTryHit},
    C(Yawn, onStart, yawn_condition_onStart),
    C(Yawn, onEnd, yawn_condition_onEnd),
    {effect(Move::Curse), Callback::onHit, &curse_onHit},
    C(Curse, onStart, curse_condition_onStart),
    C(Curse, onResidual, curse_condition_onResidual),
    {effect(Move::BellyDrum), Callback::onHit, &bellydrum_onHit},
    {effect(Move::PainSplit), Callback::onHit, &painsplit_onHit},
    {effect(Move::FakeOut), Callback::onTry, &fakeout_onTry},
    {effect(Move::BatonPass), Callback::onHit, &batonpass_onHit},
    {effect(Move::Substitute), Callback::onTryHit, &substitute_onTryHit},
    {effect(Move::Substitute), Callback::onHit, &substitute_onHit},
    C(Substitute, onStart, substitute_condition_onStart),
    C(Substitute, onEnd, substitute_condition_onEnd),
    C(Substitute, onTryPrimaryHit, substitute_condition_onTryPrimaryHit),
    {effect(Move::Protect), Callback::onPrepareHit, &stalling_onPrepareHit},
    {effect(Move::Protect), Callback::onHit, &stalling_onHit},
    {effect(Move::Detect), Callback::onPrepareHit, &stalling_onPrepareHit},
    {effect(Move::Detect), Callback::onHit, &stalling_onHit},
    {effect(Move::Endure), Callback::onPrepareHit, &stalling_onPrepareHit},
    {effect(Move::Endure), Callback::onHit, &stalling_onHit},
    C(Stall, onStart, stall_onStart),
    C(Stall, onStallMove, stall_onStallMove),
    C(Stall, onRestart, stall_onRestart),
    C(Protect, onStart, protect_condition_onStart),
    C(Protect, onTryHit, protect_condition_onTryHit),
    C(Endure, onStart, endure_condition_onStart),
    C(Endure, onDamage, endure_condition_onDamage),
    C(Spikes, onSideStart, spikes_onSideStart),
    C(Spikes, onSideRestart, spikes_onSideRestart),
    C(Spikes, onEntryHazard, spikes_onEntryHazard),
    C(Reflect, durationCallback, reflect_durationCallback),
    C(Reflect, onSideStart, reflect_onSideStart),
    C(Reflect, onSideEnd, reflect_onSideEnd),
    C(Reflect, onAnyModifyDamagePhase1, reflect_onAnyModifyDamagePhase1),
    C(LightScreen, durationCallback, lightscreen_durationCallback),
    C(LightScreen, onSideStart, lightscreen_onSideStart),
    C(LightScreen, onSideEnd, lightscreen_onSideEnd),
    C(LightScreen, onAnyModifyDamagePhase1, lightscreen_onAnyModifyDamagePhase1),
    C(Safeguard, durationCallback, safeguard_durationCallback),
    C(Safeguard, onSideStart, safeguard_onSideStart),
    C(Safeguard, onSideEnd, safeguard_onSideEnd),
    C(Safeguard, onSetStatus, safeguard_onSetStatus),
    C(Safeguard, onTryAddVolatile, safeguard_onTryAddVolatile),
    C(Mist, onSideStart, mist_onSideStart),
    C(Mist, onSideEnd, mist_onSideEnd),
    C(Mist, onTryBoost, mist_onTryBoost),
    {effect(Move::BrickBreak), Callback::onTryHit, &brickbreak_onTryHit},
    {effect(Move::RapidSpin), Callback::onAfterHit, &rapidspin_onAfterHit},
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
inline void rapidspin_self_onHit(Ctx& c, Ref pokemon) { callbacks::rapid_spin(c, pokemon); }
// Tri Attack's secondary: a burn, paralysis or a freeze, drawn.
inline void triattack_secondary_onHit(Ctx& c, Ref target, Ref source) {
  static constexpr Condition STATUS[] = {Condition::Brn, Condition::Par, Condition::Frz};
  try_set_status(c, target, STATUS[random(c.b, c.out, 3)], source);
}

} // namespace adv
