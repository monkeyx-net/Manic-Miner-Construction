#!/usr/bin/env python3
"""
Convert src/ C files:
  1. Allman braces  -> K&R braces
  2. PascalCase function names -> snake_case
"""

import re
import sys

# ---------------------------------------------------------------------------
# Rename table — sorted longest-first so longer names are replaced before
# any shorter name that is a prefix of them.
# ---------------------------------------------------------------------------
_RAW_RENAMES = [
    # EVENT globals (function-pointer variables)
    ('Spg_Drawer',              'solar_powered_generator_drawer'),
    ('Miner_Ticker',            'miner_ticker'),
    ('Miner_Drawer',            'miner_drawer'),
    ('Portal_Ticker',           'portal_ticker'),
    ('Game_DrawAir',            'game_draw_air'),
    ('Game_ExtraLife',          'game_extra_life'),
    ('Cheat_Responder',         'cheat_responder'),
    ('Action',                  'action'),
    ('Responder',               'responder'),
    ('Ticker',                  'ticker'),
    ('Drawer',                  'drawer'),

    # SaveState
    ('SaveState_ApplyPendingRestore', 'savestate_apply_pending_restore'),
    ('SaveState_HasPendingRestore',   'savestate_has_pending_restore'),
    ('SaveState_AnyExists',           'savestate_any_exists'),
    ('SaveState_Delete',              'savestate_delete'),
    ('SaveState_Exists',              'savestate_exists'),
    ('SaveState_GetInfo',             'savestate_get_info'),
    ('SaveState_Load',                'savestate_load'),
    ('SaveState_Save',                'savestate_save'),
    ('SaveState_SetPending',          'savestate_set_pending'),

    # Replay
    ('Replay_StartPlayback',    'replay_start_playback'),
    ('Replay_StartRecording',   'replay_start_recording'),
    ('Replay_ToggleR',          'replay_toggle_r'),
    ('Replay_HasBuffer',        'replay_has_buffer'),
    ('Replay_DrawOSD',          'replay_draw_osd'),
    ('Replay_Delete',           'replay_delete'),
    ('Replay_Exists',           'replay_exists'),
    ('Replay_IsKey',            'replay_is_key'),
    ('Replay_Load',             'replay_load'),
    ('Replay_Save',             'replay_save'),
    ('Replay_Tick',             'replay_tick'),

    # Video
    ('Video_ClearSprites',      'video_clear_sprites'),
    ('Video_CopyBytes',         'video_copy_bytes'),
    ('Video_CopyColour',        'video_copy_colour'),
    ('Video_LevelInkFill',      'video_level_ink_fill'),
    ('Video_LevelPaperFill',    'video_level_paper_fill'),
    ('Video_PianoKey',          'video_piano_key'),
    ('Video_PixelFill',         'video_pixel_fill'),
    ('Video_SpriteBlend',       'video_sprite_blend'),
    ('Video_SpriteOpaque',      'video_sprite_opaque'),
    ('Video_TextWidth',         'video_text_width'),
    ('Video_TileInk',           'video_tile_ink'),
    ('Video_TilePaper',         'video_tile_paper'),
    ('Video_Viewport',          'video_viewport'),
    ('Video_WriteLarge',        'video_write_large'),
    ('Video_AirBar',            'video_air_bar'),
    ('Video_CopyBytes',         'video_copy_bytes'),
    ('Video_Miner',             'video_miner'),
    ('Video_Sprite',            'video_sprite'),
    ('Video_Tile',              'video_tile'),
    ('Video_Write',             'video_write'),
    ('Video_Init',              'video_init'),

    # Game
    ('Game_IsPlaying',          'game_is_playing'),
    ('Game_IsVictory',          'game_is_victory'),
    ('Game_CheckHighScore',     'game_check_high_score'),
    ('Game_GetLevelHiScore',    'game_get_level_hi_score'),
    ('Game_SetLevelHiScore',    'game_set_level_hi_score'),
    ('Game_UpdateLevelHiScore', 'game_update_level_hi_score'),
    ('Game_StartSaveFlash',     'game_start_save_flash'),
    ('Game_ChangeLevel',        'game_change_level'),
    ('Game_DrawHiScore',        'game_draw_hi_score'),
    ('Game_DrawSaveOSD',        'game_draw_save_osd'),
    ('Game_DrawScore',          'game_draw_score'),
    ('Game_DrawLives',          'game_draw_lives'),
    ('Game_SetNumLevels',       'game_set_num_levels'),
    ('Game_GetScores',          'game_get_scores'),
    ('Game_SetScores',          'game_set_scores'),
    ('Game_ReduceAir',          'game_reduce_air'),
    ('Game_ScoreAdd',           'game_score_add'),
    ('Game_SetBorder',          'game_set_border'),
    ('Game_GameReset',          'game_reset'),
    ('Game_GotItem',            'game_got_item'),
    ('Game_SetAir',             'game_set_air'),
    ('Game_Action',             'game_action'),
    ('Game_Pause',              'game_pause'),

    # Level
    ('Level_GetRenderTile',     'level_get_render_tile'),
    ('Level_AllItemsCollected', 'level_all_items_collected'),
    ('Level_CollapseTile',      'level_collapse_tile'),
    ('Level_GetSaveData',       'level_get_save_data'),
    ('Level_GetSpgTile',        'level_get_solar_powered_generator_tile'),
    ('Level_GetTileType',       'level_get_tile_type'),
    ('Level_ItemDrawer',        'level_item_drawer'),
    ('Level_MarkTileDirty',     'level_mark_tile_dirty'),
    ('Level_ReduceItemCount',   'level_reduce_item_count'),
    ('Level_SetSaveData',       'level_set_save_data'),
    ('Level_SetSpgTile',        'level_set_solar_powered_generator_tile'),
    ('Level_SetEntry',          'level_set_entry'),
    ('Level_TileDelete',        'level_tile_delete'),
    ('Level_GetName',           'level_get_name'),
    ('Level_Drawer',            'level_drawer'),
    ('Level_GetBG',             'level_get_bg'),
    ('Level_Switch',            'level_switch'),
    ('Level_Ticker',            'level_ticker'),
    ('Level_Init',              'level_init'),

    # Miner
    ('Miner_GetRenderData',     'miner_get_render_data'),
    ('Miner_DrawSeqSprite',     'miner_draw_seq_sprite'),
    ('Miner_GetSaveData',       'miner_get_save_data'),
    ('Miner_SetSaveData',       'miner_set_save_data'),
    ('Miner_IncSeq',            'miner_inc_seq'),
    ('Miner_SetStart',          'miner_set_start'),
    ('Miner_SetSeq',            'miner_set_seq'),
    ('Miner_Init',              'miner_init'),

    # Portal
    ('Portal_GetRenderData',    'portal_get_render_data'),
    ('Portal_GetSaveData',      'portal_get_save_data'),
    ('Portal_SetSaveData',      'portal_set_save_data'),
    ('Portal_SwordFish',        'portal_sword_fish'),
    ('Portal_SetEntry',         'portal_set_entry'),
    ('Portal_Drawer',           'portal_drawer'),
    ('Portal_Ready',            'portal_ready'),
    ('Portal_Init',             'portal_init'),

    # Npcs
    ('Npcs_GetRenderData',    'npcs_get_render_data'),
    ('Npcs_ClearLevel',       'npcs_clear_level'),
    ('Npcs_GetSaveData',      'npcs_get_save_data'),
    ('Npcs_SetSaveData',      'npcs_set_save_data'),
    ('Npcs_SetSprite',        'npcs_set_sprite'),
    ('Npcs_SetStart',         'npcs_set_start'),
    ('Npcs_Version',          'npcs_version'),
    ('Npcs_Barrel',           'npcs_barrel'),
    ('Npcs_Drawer',           'npcs_drawer'),
    ('Npcs_Eugene',           'npcs_eugene'),
    ('Npcs_Ticker',           'npcs_ticker'),
    ('Npcs_Kong',             'npcs_kong'),
    ('Npcs_Init',             'npcs_init'),

    # Audio
    ('Audio_Init',              'audio_init'),
    ('Audio_Quit',              'audio_quit'),
    ('Audio_MusicEvent',        'audio_music_event'),
    ('Audio_MinerSfx',          'audio_miner_sfx'),
    ('Audio_SfxEvent',          'audio_sfx_event'),
    ('Audio_Drawer',            'audio_drawer'),
    ('Audio_Music',             'audio_music'),
    ('Audio_Output',            'audio_output'),
    ('Audio_Play',              'audio_play'),
    ('Audio_Sfx',               'audio_sfx'),

    # System / Util
    ('System_GetEvent',         'system_get_event'),
    ('System_SetPixel',         'system_set_pixel'),
    ('System_PollKey',          'system_poll_key'),
    ('System_IsKey',            'system_is_key'),
    ('System_Border',           'system_border'),
    ('Timer_Update',            'timer_update'),
    ('Timer_Set',               'timer_set'),

    # GameConfig / GameData
    ('GameConfig_Load',         'game_config_load'),
    ('GameConfig_Save',         'game_config_save'),
    ('GameData_Load',           'game_data_load'),

    # Gameover / Title / Transitions
    ('Gameover_GetRenderData',  'gameover_get_render_data'),
    ('Gameover_DrawCheat',      'gameover_draw_cheat'),
    ('Gameover_Action',         'gameover_action'),
    ('Title_ScreenCopy',        'title_screen_copy'),
    ('Title_Action',            'title_action'),
    ('Trans_Action',            'trans_action'),
    ('Victory_Action',          'victory_action'),
    ('Loader_Action',           'loader_action'),
    ('Die_Action',              'lives_action'),

    # Internal Do* — state-machine EVENT functions
    ('DoGameDemoResponder',     'do_game_demo_responder'),
    ('DoGameDrawOnce',          'do_game_draw_once'),
    ('DoGameDrawer',            'do_game_drawer'),
    ('DoGameResponder',         'do_game_responder'),
    ('DoGameoverDrawer',        'do_gameover_drawer'),
    ('DoGameoverInit',          'do_gameover_init'),
    ('DoGameoverTicker',        'do_gameover_ticker'),
    ('DoGameTicker',            'do_game_ticker'),
    ('DoGameInit',              'do_game_init'),
    ('DoTransResponder',        'do_trans_responder'),
    ('DoTransDrawer',           'do_trans_drawer'),
    ('DoTransTicker',           'do_trans_ticker'),
    ('DoTransInit',             'do_trans_init'),
    ('DoTitleDrawer',           'do_title_drawer'),
    ('DoTitleResponder',        'do_title_responder'),
    ('DoTitleTicker',           'do_title_ticker'),
    ('DoTitleAction',           'do_title_action'),
    ('DoTitleInit',             'do_title_init'),
    ('DoVictoryTicker',         'do_victory_ticker'),
    ('DoVictoryInit',           'do_victory_init'),
    ('DoDieDrawer',             'do_lives_drawer'),
    ('DoDieTicker',             'do_lives_ticker'),
    ('DoDieInit',               'do_lives_init'),
    ('DoLoaderDrawer',          'do_loader_drawer'),
    ('DoLoaderResponder',       'do_loader_responder'),
    ('DoLoaderTicker',          'do_loader_ticker'),
    ('DoLoaderInit',            'do_loader_init'),
    ('DoHiScoresResponder',     'do_hi_scores_responder'),
    ('DoOptionsResponder',      'do_options_responder'),
    ('DoOptionsInit',           'do_options_init'),
    ('DoSaveLoadResponder',     'do_save_load_responder'),
    ('DoSaveLoadInit',          'do_save_load_init'),
    ('DoMinerDrawer',           'do_miner_drawer'),
    ('DoMinerTicker',           'do_miner_ticker'),
    ('DoPortalTicker',          'do_portal_ticker'),
    ('DoSpgDrawer',             'do_solar_powered_generator_drawer'),
    ('DoStartGame',             'do_start_game'),
    ('DoCheatDisabled',         'do_cheat_disabled'),
    ('DoCheatEnabled',          'do_cheat_enabled'),

    # Internal Do* — npc movement/draw
    ('DoNpcSkylab',           'do_npc_skylab'),
    ('DoNpcEugene',           'do_npc_eugene'),
    ('DoNpcRight',            'do_npc_right'),
    ('DoNpcDown',             'do_npc_down'),
    ('DoNpcLeft',             'do_npc_left'),
    ('DoNpcFall',             'do_npc_fall'),
    ('DoNpcKong',             'do_npc_kong'),
    ('DoNpcDraw',             'do_npc_draw'),
    ('DoNpcSpg',              'do_npc_solar_powered_generator'),
    ('DoNpcUp',               'do_npc_up'),

    # Internal Do* — level tile
    ('DoWallBottom',            'do_wall_bottom'),
    ('DoWallTick',              'do_wall_tick'),
    ('DoWallTop',               'do_wall_top'),
    ('DoCollapse',              'do_collapse'),
    ('DoSpace',                 'do_space'),
    ('DoTile',                  'do_tile'),
    ('DoWall',                  'do_wall'),

    # Internal Do* — game helpers
    ('DoExtraLife',             'do_extra_life'),
    ('DoDrawAir',               'do_draw_air'),

    # HiRes
    ('HiRes_Render',            'hires_render'),
    ('HiRes_Init',              'hires_init'),
    ('HiRes_Quit',              'hires_quit'),

    # Internal helpers
    ('SaveLoadMenu_Action',     'save_load_menu_action'),
    ('GameDrawScore',           'draw_score'),
    ('DrawSaveLoadSlots',       'draw_save_load_slots'),
    ('DrawOptionsItems',        'draw_options_items'),
    ('RepaintBootSpritePaper',  'repaint_boot_sprite_paper'),
    ('ApplyDisplayMode',        'apply_display_mode'),
    ('MainLoopIteration',       'main_loop_iteration'),
    ('MoveToID',                'move_to_id'),
    ('IDToMove',                'id_to_move'),
    ('TextCode',                'text_code'),

    # Null event — used everywhere
    ('DoNothing',               'do_nothing'),
    ('DoQuit',                  'do_quit'),
]

# Sort longest old-name first to prevent partial-match replacement
RENAMES = sorted(_RAW_RENAMES, key=lambda p: -len(p[0]))


# ---------------------------------------------------------------------------
# K&R brace conversion
# ---------------------------------------------------------------------------

def _is_initializer_context(prev_stripped):
    """Return True if the standalone '{' belongs to a data initialiser."""
    return prev_stripped.endswith(('=', ',', '{', '}'))


def convert_braces(text):
    lines = text.split('\n')
    result = []

    for line in lines:
        stripped = line.strip()
        if stripped == '{' and result:
            # Find the last non-blank line we've accumulated
            prev_idx = len(result) - 1
            while prev_idx >= 0 and not result[prev_idx].strip():
                prev_idx -= 1
            if prev_idx >= 0:
                prev_stripped = result[prev_idx].rstrip().strip()
                if prev_stripped.startswith('#'):
                    pass  # never merge onto a preprocessor directive
                elif not _is_initializer_context(prev_stripped):
                    # If prev line has an inline // comment, insert '{' before it
                    prev_line = result[prev_idx].rstrip()
                    comment_pos = prev_line.find('//')
                    if comment_pos != -1:
                        result[prev_idx] = prev_line[:comment_pos].rstrip() + ' { ' + prev_line[comment_pos:]
                    else:
                        result[prev_idx] = prev_line + ' {'
                    continue   # don't append the standalone '{' line

        result.append(line)

    text = '\n'.join(result)

    # "}\n    else" -> "} else"  (handles both 'else' and 'else if')
    text = re.sub(r'\}(\n[ \t]*)else\b', '} else', text)

    # "}\nTYPENAME;" -> "} TYPENAME;"  (typedef struct/enum closing)
    text = re.sub(r'\}(\n[ \t]*)([A-Z][A-Z0-9_]+;)', r'} \2', text)

    return text


# ---------------------------------------------------------------------------
# snake_case rename
# ---------------------------------------------------------------------------

def convert_names(text):
    for old, new in RENAMES:
        text = re.sub(r'\b' + re.escape(old) + r'\b', new, text)
    return text


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def convert_file(path):
    with open(path, 'r', encoding='utf-8') as fh:
        text = fh.read()
    text = convert_braces(text)
    text = convert_names(text)
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write(text)


if __name__ == '__main__':
    paths = sys.argv[1:]
    if not paths:
        print('usage: convert_style.py <file> [<file> ...]')
        sys.exit(1)
    for path in paths:
        convert_file(path)
        print(f'  converted {path}')
    print(f'done ({len(paths)} files)')
