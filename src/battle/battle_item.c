#include "../../include/battle.h"
#include "../../include/debug.h"
#include "../../include/pokemon.h"
#include "../../include/types.h"
#include "../../include/constants/ability.h"
#include "../../include/constants/battle_script_constants.h"
#include "../../include/constants/hold_item_effects.h"
#include "../../include/constants/item.h"
#include "../../include/constants/move_effects.h"
#include "../../include/constants/moves.h"
#include "../../include/constants/species.h"
#include "../../include/constants/weather_numbers.h"
#include "../../include/constants/battle_message_constants.h"
#include "../../include/constants/file.h"


//this function is for held item effects for when U-Turn is used
//if you want to edit a defender's held item effect triggering after being hit, go to CheckDefenderItemEffectOnHit
//if you want to edit an attacker's held item effect triggering after hitting using a move, go to ServerWazaHitAfterCheckAct
u32 MoveHitUTurnHeldItemEffectCheck(void *bw, struct BattleStruct *sp, int *seq_no)
{
    u32 ret;
    int client_no;
    int atk_hold_eff;
    int atk_item_param;
    int def_hold_eff;
    int def_item_param;
    int atk_side;
    
    ret = FALSE;

    atk_hold_eff = HeldItemHoldEffectGet(sp, sp->attack_client);
    atk_item_param = HeldItemAtkGet(sp,sp->attack_client, ATK_CHECK_NORMAL);

    def_hold_eff = HeldItemHoldEffectGet(sp, sp->defence_client);
    def_item_param = HeldItemAtkGet(sp,sp->defence_client, ATK_CHECK_NORMAL);

    atk_side = IsClientEnemy(bw, sp->attack_client);

    if ((atk_hold_eff == HOLD_EFFECT_HP_RESTORE_ON_DMG) // shell bell
     && (sp->server_status_flag & SERVER_STATUS_FLAG_MOVE_HIT)
     && (sp->oneSelfFlag[sp->attack_client].shell_bell_damage)
     && (sp->attack_client != sp->defence_client)
     && (sp->battlemon[sp->attack_client].hp < sp->battlemon[sp->attack_client].maxhp)
     && (sp->battlemon[sp->attack_client].hp)
     && !(GetBattlerAbility(sp, sp->attack_client) == ABILITY_SHEER_FORCE && sp->battlemon[sp->attack_client].sheer_force_flag == 1)) // sheer force prevents shell bell from activating
    {
        sp->hp_calc_work = BattleDamageDivide(sp->oneSelfFlag[sp->attack_client].shell_bell_damage * -1, atk_item_param);
        sp->client_work = sp->attack_client;
        seq_no[0] = SUB_SEQ_SHELL_BELL_HEAL;
        ret = TRUE;
    }

    if ((atk_hold_eff == HOLD_EFFECT_HP_DRAIN_ON_ATK) // life orb
     && (GetBattlerAbility(sp, sp->attack_client) != ABILITY_MAGIC_GUARD)
     && (sp->server_status_flag & SERVER_STATUS_FLAG_MOVE_HIT)
     && (sp->moveTbl[sp->current_move_index].split != SPLIT_STATUS)
     && (sp->battlemon[sp->attack_client].hp)
     && !(GetBattlerAbility(sp, sp->attack_client) == ABILITY_SHEER_FORCE && sp->battlemon[sp->attack_client].sheer_force_flag == 1)) // sheer force prevents life orb from activating
    {
        sp->hp_calc_work = BattleDamageDivide(sp->battlemon[sp->attack_client].maxhp * -1, 10);
        sp->client_work = sp->attack_client;
        seq_no[0] = SUB_SEQ_LIFE_ORB;
        ret = TRUE;
    }

    if ((def_hold_eff == HOLD_EFFECT_RECOIL_PHYSICAL) // jaboca berry
     && (sp->battlemon[sp->attack_client].hp)
     && (GetBattlerAbility(sp, sp->attack_client) != ABILITY_MAGIC_GUARD)
     && (sp->oneSelfFlag[sp->defence_client].physical_damage))
    {
        sp->hp_calc_work = BattleDamageDivide(sp->battlemon[sp->attack_client].maxhp * -1, def_item_param);
        seq_no[0] = SUB_SEQ_PHYSICAL_DMG_RECOIL;
        ret = TRUE;
    }

    if ((def_hold_eff == HOLD_EFFECT_DAMAGE_HOLDER_TRANSFER_ON_CONTACT) // sticky barb
     && (sp->battlemon[sp->attack_client].hp)
        && (sp->battlemon[sp->attack_client].item == 0)
        && ((sp->scw[atk_side].knockoff_item & (1 << sp->sel_mons_no[sp->attack_client])) == 0)
        && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
      || (sp->oneSelfFlag[sp->defence_client].special_damage))
        && (sp->moveTbl[sp->current_move_index].flag & FLAG_CONTACT))
    {
        seq_no[0] = SUB_SEQ_TRANSFER_STICKY_BARB;
        ret = TRUE;
    }

    return ret;
}


enum
{
	SWHAC_RAGE_ATTACK_CHECK=0,
	SWHAC_HELD_ITEM_SHELL_BELL,
	SWHAC_HELD_ITEM_LIFE_ORB,
	SWHAC_END
};

//go to CheckDefenderItemEffectOnHit if you want to program an effect to happen after being hit for a defender's held item
//this function is for an attacker's held item effect triggering after hitting with a move
u32 ServerWazaHitAfterCheckAct(void *bw, struct BattleStruct *sp)
{
    int ret;
    int client_no;
    int client_set_max;
    int hold_effect;
    int hold_effect_param;
    
    ret = 0;

    client_set_max = BattleWorkClientSetMaxGet(bw);

    hold_effect = HeldItemHoldEffectGet(sp, sp->attack_client);
    hold_effect_param = HeldItemAtkGet(sp, sp->attack_client, ATK_CHECK_NORMAL);

    if (ServerKizetsuCheck(sp, sp->server_seq_no, sp->server_seq_no, 1) == TRUE)
    {
        return TRUE;
    }

    do
    {
        switch(sp->swhac_seq_no)
        {
        case SWHAC_RAGE_ATTACK_CHECK:
            if ((sp->battlemon[sp->attack_client].condition2 & STATUS2_FLAG_RAGE) && (sp->current_move_index != MOVE_RAGE))
            {
                sp->battlemon[sp->attack_client].condition2 &= (STATUS2_FLAG_RAGE ^ 0xFFFFFFFF);
            }
            sp->swhac_seq_no++;
            
            if (GetBattlerAbility(sp,sp->attack_client) == ABILITY_SHEER_FORCE && sp->battlemon[sp->attack_client].sheer_force_flag == 1) // skip over shell bell and life orb if sheer force is active
                sp->swhac_seq_no = SWHAC_END;

            break;
        case SWHAC_HELD_ITEM_SHELL_BELL:
            if(sp->defence_client != 0xFF)
            {
                if ((hold_effect == HOLD_EFFECT_HP_RESTORE_ON_DMG)
                 && ((sp->server_status_flag2 & SERVER_STATUS2_FLAG_x10) == 0)
                 && (sp->server_status_flag & SERVER_STATUS_FLAG_MOVE_HIT)
                 && (sp->oneSelfFlag[sp->attack_client].shell_bell_damage)
                 && (sp->attack_client != sp->defence_client)
                 && (sp->battlemon[sp->attack_client].hp < sp->battlemon[sp->attack_client].maxhp)
                 && (sp->battlemon[sp->attack_client].hp))
                {
                    sp->hp_calc_work = BattleDamageDivide(sp->oneSelfFlag[sp->attack_client].shell_bell_damage * -1, hold_effect_param);
                    sp->client_work=sp->attack_client;
                    LoadBattleSubSeqScript(sp, FILE_BATTLE_SUB_SCRIPTS, SUB_SEQ_SHELL_BELL_HEAL);
                    sp->next_server_seq_no = sp->server_seq_no;
                    sp->server_seq_no = 22;
                    ret = 1;
                }
            }
            sp->swhac_seq_no++;
            break;
        case SWHAC_HELD_ITEM_LIFE_ORB:
            if ((hold_effect == HOLD_EFFECT_HP_DRAIN_ON_ATK)
             && (GetBattlerAbility(sp,sp->attack_client) != ABILITY_MAGIC_GUARD)
             && ((sp->server_status_flag2 & SERVER_STATUS2_FLAG_x10) == 0)
             && (sp->server_status_flag & SERVER_STATUS_FLAG_MOVE_HIT)
             && (sp->moveTbl[sp->current_move_index].split != SPLIT_STATUS)
             && (sp->battlemon[sp->attack_client].hp))
            {
                sp->hp_calc_work = BattleDamageDivide(sp->battlemon[sp->attack_client].maxhp * -1, 10);
                sp->client_work = sp->attack_client;
                LoadBattleSubSeqScript(sp, FILE_BATTLE_SUB_SCRIPTS, SUB_SEQ_LIFE_ORB);
                sp->next_server_seq_no = sp->server_seq_no;
                sp->server_seq_no = 22;
                ret = 1;
            }
            sp->swhac_seq_no++;
            break;
        case SWHAC_END:
            sp->swhac_seq_no = 0;
            sp->swhac_work = 0;
            ret = 2;
            break;
        }
    }
    while (ret == 0);

    return (ret == 1);
}


//thanks to Lhea for this function - TODO ask for their SUB_SEQ scripts for the gen5+ item effects since rn those can't be used
//this function is for a defender's held item effect triggering after being hit
//go to ServerWazaHitAfterCheckAct for implementing an attacker's held item effect triggering after using a move
BOOL CheckDefenderItemEffectOnHit(void *bw, struct BattleStruct *sp, int *seq_no)
{
    BOOL ret = FALSE;

    if (sp->defence_client == 0xFF) {
        return ret;
    }

    if (CheckSubstitute(sp, sp->defence_client) == TRUE) {
        return ret;
    }

    // Check for defender's items
    int itemHoldEffect = HeldItemHoldEffectGet(sp, sp->defence_client);
    int itemPower      = HeldItemAtkGet(sp, sp->defence_client, 0);
    int side           = IsClientEnemy(bw, sp->attack_client);
    switch (itemHoldEffect) {
        // vanilla gen4 effects
        case HOLD_EFFECT_DAMAGE_HOLDER_TRANSFER_ON_CONTACT:     // Sticky Barb
            // Attacker is alive after the attack
            if ((sp->battlemon[sp->attack_client].hp)
                // Attacker has no item
                && (sp->battlemon[sp->attack_client].item == ITEM_NONE)
                // This side did not just get its item knocked off by the attacker
                && ((sp->scw[side].knockoff_item & No2Bit(sp->sel_mons_no[sp->attack_client])) == 0)
                // The move that triggered this was not Knock Off
                && (sp->current_move_index != MOVE_KNOCK_OFF)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))
                // Attacker is not U-turning
                && ((sp->server_status_flag2 & SERVER_STATUS_FLAG2_U_TURN) == 0)
                // Attacker used a move that makes contact
                && (sp->moveTbl[sp->current_move_index].flag & FLAG_CONTACT)) {
                seq_no[0] = SUB_SEQ_TRANSFER_STICKY_BARB;
                ret       = TRUE;
            }
            break;

        case HOLD_EFFECT_RECOIL_PHYSICAL:                       // Jaboca Berry
            // Attacker is alive after the attack
            if ((sp->battlemon[sp->attack_client].hp)
                // Attacker does not have Magic Guard
                && (GetBattlerAbility(sp, sp->attack_client) != ABILITY_MAGIC_GUARD)
                // Attacker is not U-turning
                && ((sp->server_status_flag2 & SERVER_STATUS_FLAG2_U_TURN) == 0)
                // Attacker dealt physical damage
                && (sp->oneSelfFlag[sp->defence_client].physical_damage)) {
                sp->hp_calc_work = BattleDamageDivide(sp->battlemon[sp->attack_client].maxhp * -1, itemPower);
                seq_no[0]                = SUB_SEQ_PHYSICAL_DMG_RECOIL;
                ret                      = TRUE;
            }
            break;

        case HOLD_EFFECT_RECOIL_SPECIAL:                        // Rowap Berry
            // Attacker is alive after the attack
            if ((sp->battlemon[sp->attack_client].hp)
                // Attacker does not have Magic Guard
                && (GetBattlerAbility(sp, sp->attack_client) != ABILITY_MAGIC_GUARD)
                // Attacker is not U-turning
                && ((sp->server_status_flag2 & SERVER_STATUS_FLAG2_U_TURN) == 0)
                // Attacker dealt special damage
                && (sp->oneSelfFlag[sp->defence_client].special_damage)) {
                sp->hp_calc_work = BattleDamageDivide(sp->battlemon[sp->attack_client].maxhp * -1, itemPower);
                seq_no[0]                = SUB_SEQ_PHYSICAL_DMG_RECOIL;
                ret                      = TRUE;
            }
            break;

        case HOLD_EFFECT_RESTORE_HP_ON_SE:                      // Enigma Berry
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Defender was hit by a Super Effective attack
                && (sp->waza_status_flag & MOVE_STATUS_FLAG_SUPER_EFFECTIVE)) {
                sp->client_work = sp->defence_client;
                sp->item_work   = sp->battlemon[sp->defence_client].item;
                seq_no[0]                = SUB_SEQ_HANDLE_ITEM_RESTORE_HP;
                ret                      = TRUE;
            }
            break;

            //these effects are not usable at all yet
#ifdef LATER_GEN_ITEM_EFFECTS
            // gen5 effects
        case HOLD_EFFECT_BOOST_SPA_ON_WATER_HIT:                // Absorb Bulb
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Defender was hit by a Water-type attack
                && (sp->moveTbl[sp->current_move_index].type == TYPE_WATER)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))
                // Defender has less than +6 stages to Special Attack
                && ((sp->battlemon[sp->defence_client].states[STAT_SPATK] < 12)
                    // Or the defender has Contrary and more than -6 stages to Special Attack
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && (sp->battlemon[sp->defence_client].states[STAT_SPATK] > 0)))) {
                sp->addeffect_type   = ADD_EFFECT_HELD_ITEM;
                sp->addeffect_param  = ADD_STATE_SP_ATK_UP;
                sp->state_client = sp->defence_client;
                sp->item_work        = sp->battlemon[sp->defence_client].item;
                seq_no[0]                     = SUB_SEQ_STAT_STAGE_CHANGE;
                ret                           = TRUE;
            }
            break;

        case HOLD_EFFECT_UNGROUND_DESTROYED_ON_HIT:             // Air Balloon
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))) {
                seq_no[0] = SUB_SEQ_HANDLE_AIR_BALLOON_POP;
                ret       = TRUE;
            }
            break;

        case HOLD_EFFECT_BOOST_ATK_ON_ELECTRIC_HIT:             // Cell Battery
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Defender was hit by an Electric-type attack
                && (sp->moveTbl[sp->current_move_index].type == TYPE_ELECTRIC)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))
                // Defender has less than +6 stages to Attack
                && ((sp->battlemon[sp->defence_client].states[STAT_ATTACK] < 12)
                    // Or the defender has Contrary and more than -6 stages to Attack
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && (sp->battlemon[sp->defence_client].states[STAT_ATTACK] > 0)))) {
                sp->addeffect_type   = ADD_EFFECT_HELD_ITEM;
                sp->addeffect_param  = ADD_STATE_ATTACK_UP;
                sp->state_client = sp->defence_client;
                sp->item_work        = sp->battlemon[sp->defence_client].item;
                seq_no[0]                     = SUB_SEQ_STAT_STAGE_CHANGE;
                ret                           = TRUE;
            }
            break;

        case HOLD_EFFECT_SWITCH_OUT_WHEN_HIT:                   // Eject Button
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))) {
                seq_no[0] = SUB_SEQ_HANDLE_EJECT_BUTTON;
                ret       = TRUE;
            }
            break;

        case HOLD_EFFECT_FORCE_SWITCH_ON_DAMAGE:                // Red Card
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))) {
                seq_no[0] = SUB_SEQ_HANDLE_RED_CARD;
                ret       = TRUE;
            }
            break;

        case HOLD_EFFECT_DAMAGE_ON_CONTACT:                     // Rocky Helmet
            // Attacker is alive after the attack
            if ((sp->battlemon[sp->attack_client].hp)
                // Attacker does not have Magic Guard
                && (GetBattlerAbility(sp, sp->attack_client) != ABILITY_MAGIC_GUARD)
                // Attacker is not holding an item that prevents contact effects, e.g. Protective Pads
                && (GetHeldItemHoldEffect(sp, sp->attack_client) != HOLD_EFFECT_PREVENT_CONTACT_EFFECTS)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))
                // Attacker used a move that makes contact
                && (sp->moveTbl[sp->current_move_index].flag & FLAG_CONTACT)) {
                sp->hp_calc_work = BattleDamageDivide(sp->battlemon[sp->attack_client].maxhp * -1, itemPower);
                seq_no[0]                = SUB_SEQ_PHYSICAL_DMG_RECOIL;
                ret                      = TRUE;
            }
            break;

            // gen6 effects
        case HOLD_EFFECT_BOOST_DEF_ON_PHYSICAL_HIT:             // Kee Berry
            // Defender is alive after the attack
            if ((sp->battlemon[sp->attack_client].hp)
                // Attacker dealt physical damage
                && (sp->oneSelfFlag[sp->defence_client].physical_damage)
                // Defender has less than +6 stages to Defense
                && ((sp->battlemon[sp->defence_client].states[STAT_DEFENSE] < 12)
                    // Or the defender has Contrary and more than -6 stages to Defense
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && (sp->battlemon[sp->defence_client].states[STAT_DEFENSE] > 0)))) {
                sp->addeffect_type   = ADD_EFFECT_HELD_ITEM;
                sp->addeffect_param  = ADD_STATE_DEFENSE_UP;
                sp->state_client = sp->defence_client;
                sp->item_work        = sp->battlemon[sp->defence_client].item;
                seq_no[0]                     = SUB_SEQ_STAT_STAGE_CHANGE;
                ret                           = TRUE;
            }
            break;

        case HOLD_EFFECT_BOOST_SPD_ON_WATER_HIT:                // Luminous Moss
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Defender was hit by a Water-type attack
                && (sp->moveTbl[sp->current_move_index].type == TYPE_WATER)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))
                // Defender has less than +6 stages to Special Defense
                && ((sp->battlemon[sp->defence_client].states[STAT_SPDEF] < 12)
                    // Or the defender has Contrary and more than -6 stages to Special Defense
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && (sp->battlemon[sp->defence_client].states[STAT_SPDEF] > 0)))) {
                sp->addeffect_type   = ADD_EFFECT_HELD_ITEM;
                sp->addeffect_param  = ADD_STATE_SP_DEF_UP;
                sp->state_client = sp->defence_client;
                sp->item_work        = sp->battlemon[sp->defence_client].item;
                seq_no[0]                     = SUB_SEQ_STAT_STAGE_CHANGE;
                ret                           = TRUE;
            }
            break;

        case HOLD_EFFECT_BOOST_SPD_ON_SPECIAL_HIT:              // Maranga Berry
            // Defender is alive after the attack
            if ((sp->battlemon[sp->attack_client].hp)
                // Attacker dealt special damage
                && (sp->oneSelfFlag[sp->defence_client].special_damage)
                // Defender has less than +6 stages to Special Defense
                && ((sp->battlemon[sp->defence_client].states[STAT_SPDEF] < 12)
                    // Or the defender has Contrary and more than -6 stages to Special Defense
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && (sp->battlemon[sp->defence_client].states[STAT_SPDEF] > 0)))) {
                sp->addeffect_type   = ADD_EFFECT_HELD_ITEM;
                sp->addeffect_param  = ADD_STATE_SP_DEF_UP;
                sp->state_client = sp->defence_client;
                sp->item_work        = sp->battlemon[sp->defence_client].item;
                seq_no[0]                     = SUB_SEQ_STAT_STAGE_CHANGE;
                ret                           = TRUE;
            }
            break;

        case HOLD_EFFECT_BOOST_ATK_ON_ICE_HIT:                  // Snowball
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Defender was hit by an Ice-type attack
                && (sp->moveTbl[sp->current_move_index].type == TYPE_ICE)
                // Damage was dealt
                && ((sp->oneSelfFlag[sp->defence_client].physical_damage)
                    || (sp->oneSelfFlag[sp->defence_client].special_damage))
                // Defender has less than +6 stages to Attack
                && ((sp->battlemon[sp->defence_client].states[STAT_ATTACK] < 12)
                    // Or the defender has Contrary and more than -6 stages to Attack
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && (sp->battlemon[sp->defence_client].states[STAT_ATTACK] > 0)))) {
                sp->addeffect_type   = ADD_EFFECT_HELD_ITEM;
                sp->addeffect_param  = ADD_STATE_ATTACK_UP;
                sp->state_client = sp->defence_client;
                sp->item_work        = sp->battlemon[sp->defence_client].item;
                seq_no[0]                     = SUB_SEQ_STAT_STAGE_CHANGE;
                ret                           = TRUE;
            }
            break;

        case HOLD_EFFECT_BOOST_ATK_AND_SPATK_ON_SE:             // Weakness Policy
            // Defender is alive after the attack
            if ((sp->battlemon[sp->defence_client].hp)
                // Defender was hit by a Super Effective attack
                && (sp->waza_status_flag & MOVE_STATUS_FLAG_SUPER_EFFECTIVE)
                // Defender has less than +6 stages to either of Attack or Special Attack
                && (((sp->battlemon[sp->defence_client].states[STAT_ATTACK] < 12)
                     || (sp->battlemon[sp->defence_client].states[STAT_SPATK] < 12))
                    // Or the defender has Contrary and more than -6 stages to either of Attack or Special Attack
                    || ((GetBattlerAbility(sp, sp->defence_client) == ABILITY_CONTRARY)
                        && ((sp->battlemon[sp->defence_client].states[STAT_ATTACK] > 0)
                            || (sp->battlemon[sp->defence_client].states[STAT_SPATK] > 0))))) {
                seq_no[0] = SUB_SEQ_HANDLE_WEAKNESS_POLICY;
                ret       = TRUE;
            }
            break;

            // gen7 has no distinct standalone effects that go here
            // Protective Pads is dispersed among the relevant cases
            // (here, only Rocky Helmet)

            // gen8 has no effects which belong in this particular
            // switch tree, since everything here cares about being
            // *dealt* damage, rather than *dealing* damage

#endif
        default:
            break;
    }


    return ret;
}

bool8 CheckItemByThief(u16 item)
{
    if (item == ITEM_GRISEOUS_ORB || IS_ITEM_MEGA_STONE(item))
        return TRUE;
    return FALSE;
}
