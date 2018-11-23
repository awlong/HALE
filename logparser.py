import numpy as np
import json
import copy

STATE_MAP = {'L':0, 'T':1, 'W':2, 'A':3, 'F':4, 'I':5, 'C':6, '#':7}
CHAIN_MAP = {
    'LUXOR':0, 0:'LUXOR',
    'TOWER':1, 1:'TOWER',
    'WORLDWIDE':2, 2:'WORLDWIDE',
    'AMERICAN':3, 3:'AMERICAN',
    'FESTIVAL':4, 4:'FESTIVAL',
    'IMPERIAL':5, 5:'IMPERIAL',
    'CONTINENTAL':6, 6:'CONTINENTAL',
    'UNCONNECTED':7, 7:'UNCONNECTED'}

# conver the board indexing into other reflected spaces
HORIZ_BOARD = np.arange(9*12).reshape(9,12)[:,::-1].flatten()
VERT_BOARD = np.arange(9*12).reshape(9,12)[::-1,:].flatten()
HORIZ_VERT_BOARD = np.arange(9*12).reshape(9,12)[::-1,::-1].flatten()

def parse_log(path):
    with open(path,'r') as f:
        file_contents = f.read()
    contents = file_contents.splitlines()

    turn_starts = [i for i,line in enumerate(contents) if line == 'TURN START']
    turn_starts = turn_starts + [i for i,line in enumerate(contents) if line == 'runGame: END OF GAME']
    
    game_log = {}
    game_log['start_state'] = parse_header(contents[:turn_starts[0]])
    turns = []
    for turn_id in range(len(turn_starts)-1):
        action, state = parse_turn(contents[turn_starts[turn_id]:turn_starts[turn_id+1]])
        turns.append({'action':action,'state':state})
    game_log['turns'] = turns
    
    game_log['end_state'] = parse_end_state(contents[turn_starts[-1]:])
    return game_log

def parse_header(contents):
    state = {}
    state['starting_player'] = contents[0].split(':')[2]
    state['board'] = parse_board(contents[1:10])
    state = add_players_to_state(state, contents, 11)
    return state

def parse_state(contents):
    board_start = [i+1 for i,line in enumerate(contents) if 'canEndGame' in line][0]
    board_end = board_start + 9
    state = {}
    state['board'] = parse_board(contents[board_start:board_end])
    state = add_players_to_state(state, contents, board_end+1)
    return state 
    
def parse_board(contents):
    board = np.zeros((8,9,12), np.uint8)
    for i,line in enumerate(contents):
        states = line.split(' ')
        for j, state in enumerate(states[:-1]):
            if state != '.':
                board[STATE_MAP[state], i, j] = 1
    return board

def parse_player(contents):
    player = {}
    player['name'] = contents[0][10:-1]
    player['cash'] = int(contents[1][1:])
    player['tiles'] = [int(i) for i in contents[2].strip().split(' ')[1:]]
    player['stocks'] = [int(i) for i in contents[3].strip().split(' ')[1:]]
    player['value'] = int(contents[4].split('$')[1])
    return player

def add_players_to_state(state, contents, start):
    end = start+5
    for player_id in range(4):
        state[player_id] = parse_player(contents[start:end])
        start = end
        end = start + 5
    return state


def parse_actions(contents):
    actions = {}
    # tile playing
    playing_lines = [line for line in contents if 'handleTilePlayPhase' in line]
    for line in playing_lines:
        if 'Player Number' in line:
            actions['player'] = int(line.split(':')[-1])
        if 'Playing tile' in line:
            actions['tile'] = int(line.split(':')[-1])
    
    # logging merge choices    
    merger_lines = [line for line in contents if 'handleTilePlayMerger' in line]
    if len(merger_lines) > 0:
        merge_action = {}
        merge_chains = []
        for line in merger_lines:
            if 'Merging Chains' in line:
                merge_chains.append(CHAIN_MAP[line.split(':')[-1].strip()])
            if 'Surviving chain' in line:
                survivor = CHAIN_MAP[line.split(':')[-1].strip()]
        merge_trade_index = [i for i,line in enumerate(contents) if 'Player Merge Actions:' in line]
        player_actions = []
        for start_index in merge_trade_index:
            player = {}
            player['player'] = int(contents[start_index].split(':')[-1])
            player['sell'] = int(contents[start_index+1].split(':')[-1])
            player['trade'] = int(contents[start_index+2].split(':')[-1])
            player_actions.append(player)
        merge_actions = {'chains':merge_chains, 'survivor':survivor, 'player_actions': player_actions}
        actions['merge'] = merge_actions

    # logging chain creation
    create_lines = [line for line in contents if 'Create' in line]
    if len(create_lines) > 0:
        chain = CHAIN_MAP[create_lines[0].split(':')[-1].strip()]
        if len(create_lines) == 2:
            share = True
        else:
            share = False
        create_action = {'chain':chain, 'share':share}
        actions['create'] = create_action

    # logging share purchases
    share_purchase_lines = [line for line in contents if 'SharePurchasePhase' in line]
    if len(share_purchase_lines) > 0:
        share_actions = {}
        for line in share_purchase_lines:
            tokens = line.split(':')
            share_actions[CHAIN_MAP[tokens[1].strip()]] = int(tokens[2]) 
        actions['share'] = share_actions
    return actions 

def parse_turn(contents):
    actions = parse_actions(contents)
    state = parse_state(contents)
    return actions, state
    
def parse_end_state(contents):
    state = {}
    state['winner'] = int(contents[1].split(':')[-1])
    state['value'] = int(contents[2].split(':')[-1])
    board_start = 3
    board_end = board_start + 9
    state['board'] = parse_board(contents[board_start:board_end])
    state = add_players_to_state(state, contents, board_end+1)
    return state


class NumpyEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        if isinstance(obj, np.int64):
            return int(obj)
        return json.JSONEncoder.default(self, obj)

def permute_state(state, permutation, horizontal, vertical):
        state['board'] = permute_board(state['board'], permutation, horizontal, vertical)
        for i in range(4):
            state[i]['stocks'] = np.take(state[i]['stocks'], permutation[:-1])
            state[i]['tiles'] = reflect_tile(state[i]['tiles'], horizontal, vertical)
        return state

def permute_action(action, permutation, horizontal, vertical):
    action['tile'] = reflect_tile(action['tile'], horizontal, vertical)
    if 'create' in action.keys():
        action['create']['chain'] = permutation[action['create']['chain']]
    if 'share' in action.keys():
        shares = {}
        for share_key in action['share'].keys():
            share = int(share_key)
            shares[permutation[share]] = action['share'][share_key]
        action['share'] = shares
    if 'merge' in action.keys():
        chains = []
        for chain in action['merge']['chains']:
            chains.append(permutation[chain])
        action['merge']['chains'] = chains
        action['merge']['survivor'] = permutation[action['merge']['survivor']]
    return action

def permute_gamelog(aug_log, permutation, horizontal=False, vertical=False): 
    aug_log['start_state'] = permute_state(aug_log['start_state'], permutation, horizontal, vertical)
    for turn_id in range(len(game_log['turns'])):
        aug_log['turns'][turn_id]['state'] = permute_state(aug_log['turns'][turn_id]['state'], permutation, horizontal, vertical)
        aug_log['turns'][turn_id]['action'] = permute_action(aug_log['turns'][turn_id]['action'], permutation, horizontal, vertical)      
    aug_log['end_state'] = permute_state(aug_log['end_state'], permutation, horizontal, vertical)
    return aug_log

def reflect_tile(tile, horizontal, vertical):
    if isinstance(tile, list):
        return [reflect_tile(x, horizontal, vertical) for x in tile]
    if tile == 255:
        return tile
    if not horizontal and not vertical:
        return tile
    elif horizontal and not vertical:
        return HORIZ_BOARD[tile]
    elif not horizontal and vertical:
        return VERT_BOARD[tile]
    else:
        return HORIZ_VERT_BOARD[tile]

def permute_board(board, permutation, horizontal, vertical):
    board = np.take(board, permutation, axis=0)
    if horizontal:
        board = board[:,:,::-1]
    if vertical:
        board = board[:,::-1,:]
    return board

def construct_all_permuted_logs(game_log):
    # permute the L/T - [0,1], [1,0] (2)
    # permute the W/A/F - [2,3,4],[2,4,3],[3,2,4],[3,4,2],[4,2,3],[4,3,2] (6)
    # permute the I/C - [5,6], [6,5] (2)
    # reflect left/right - (2)
    # reflect top/bottom - (2)
    game_logs = []
    for LT in [[0,1],[1,0]]:
        for WAF in [[2,3,4],[2,4,3],[3,2,4],[3,4,2],[4,2,3],[4,3,2]]:
            for IC in [[5,6], [6,5]]:
                permutation = LT+WAF+IC+[7]
                for horizontal in [False, True]:
                    for vertical in [False, True]:
                        aug_log = permute_gamelog(copy.deepcopy(game_log), permutation, horizontal, vertical)
                        game_logs.append(aug_log)
    return game_logs

if __name__ == '__main__':
    import sys
    import os
    
    if len(sys.argv) != 3:
        print("usage: python {} file_path save_path")
    file_path = sys.argv[1]
    file_base = os.path.splitext(os.path.split(file_path)[-1])[0]

    save_base = os.path.join(sys.argv[2], file_base)
    game_log = parse_log(file_path)
    augmented_logs = construct_all_permuted_logs(game_log)

    import pickle
    with open('{}.pik'.format(save_base), 'wb') as f:
        pickle.dump(augmented_logs, f)

    # for aug_id,log in enumerate(augmented_logs):
    #     with open('{}_{}.pickle'.format(save_base, aug_id), 'w') as f:
    #         json.dump(log, f, cls=NumpyEncoder, separators=(',',':'), indent=4)

    
