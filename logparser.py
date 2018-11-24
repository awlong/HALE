import numpy as np
import copy

###################
# PARSING FUNCTIONS
###################

def parse_log(path):
    '''Function takes the path for a game log file and creates a dictionary object
    
    Args:
        path (str): path to the log file to be parsed
    
    Returns:
        dict: dictionary describing the full game log ('start_state','turns':[{action,state}], 'end_state')
    '''

    with open(path,'r') as f:
        file_contents = f.read()
    contents = file_contents.splitlines()

    # identify where the turns occur
    turn_starts = [i for i,line in enumerate(contents) if line == 'TURN START']
    turn_starts = turn_starts + [i for i,line in enumerate(contents) if line == 'runGame: END OF GAME']
    
    # beginning game state
    game_log = {}
    game_log['start_state'] = parse_start_state(contents[:turn_starts[0]])
    # parse all the individual turns
    turns = []
    for turn_id in range(len(turn_starts)-1):
        action, state = parse_turn(contents[turn_starts[turn_id]:turn_starts[turn_id+1]])
        turns.append({'action':action,'state':state})
    game_log['turns'] = turns
    # ending game state
    game_log['end_state'] = parse_end_state(contents[turn_starts[-1]:])
    return game_log

def parse_state(contents, start):
    '''Takes a set of lines describing a turn and builds a state dict for the post-turn state.
    
    Args:
        contents (list[str]): lines from the log file for the given turn to be parsed
        start (int): index for where the state starts (where the board starts)
    Returns:
        dict: describes the game state from the given turn ('board', 0-3 where the # is the player state)
    '''

    end = start + 9
    state = {}
    state['board'] = parse_board(contents[start:end])
    # add the players to the state
    start = end+1
    end = start+5
    for player_id in range(4):
        state[player_id] = parse_player(contents[start:end])
        start = end
        end = start + 5
    return state 
    
def parse_board(contents):
    '''Builds the board np.ndarray from the lines describing the board. Each layer (axis=0) is a different chain type (one-hot encoding of the locations).
    
    Args:
        contents (list[str]): lines for the board state
    
    Returns:
        np.ndarray[8,9,12] : board ndarray with layers [L,T,W,A,F,I,C,#]
    '''

    board = np.zeros((8,9,12), np.uint8)
    for i,line in enumerate(contents):
        states = line.split(' ')
        for j, state in enumerate(states[:-1]):
            if state != '.':
                board[STATE_MAP[state], i, j] = 1
    return board

def parse_player(contents):
    '''Parses the player data from a set of log file lines.
    
    Args:
        contents (list[str]): lines for the player state
    
    Returns:
        dict: dictionary describing the player state ('name','cash','tiles','stocks','value')
    '''

    player = {}
    player['name'] = contents[0][10:-1]
    player['cash'] = int(contents[1][1:])
    player['tiles'] = [int(i) for i in contents[2].strip().split(' ')[1:]]
    player['stocks'] = [int(i) for i in contents[3].strip().split(' ')[1:]]
    player['value'] = int(contents[4].split('$')[1])
    return player

def parse_actions(contents):
    '''Parse the turn actions performed by the player. Action dictionary has a distinct hierarchy (if a key does not exist no relevant action was taken)
    action:
    - 'player': current player number for action
    - 'tile': number of the tile played
    - 'merge':
        - 'chains': list of chains involved in the merger
        - 'survivor': surviving chain id after the merger
        - 'player_actions': list of player action dicts
            - 'player': player index for merge action
            - 'sell': how many shares were sold
            - 'trade': how many shares of the new stock were acquired via trade
    - 'create':
        - 'chain': chain number to create
        - 'share': True/False, whether a share was given to the player
    - 'share': dictionary mapping hotel index to quantity of stock purchased

    Args:
        contents (list[str]): list of log file lines
    
    Returns:
        dict: dictionary describing the action performed by the current player/actions done by players during merging/etc.
    '''

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

    # logging share purchases,
    share_purchase_lines = [line for line in contents if 'SharePurchasePhase' in line]
    if len(share_purchase_lines) > 0:
        share_actions = {}
        for line in share_purchase_lines:
            tokens = line.split(':')
            share_actions[CHAIN_MAP[tokens[1].strip()]] = int(tokens[2]) 
        actions['share'] = share_actions
    return actions 

def parse_turn(contents):
    '''Parses a turn (action + resulting state) from the list of log file lines.
    
    Args:
        contents (list[str]): lines from the log file
    
    Returns:
        tuple: tuple of the actions and state
    '''

    actions = parse_actions(contents)
    state_start = [i+1 for i,line in enumerate(contents) if 'canEndGame' in line][0]
    state = parse_state(contents, state_start)
    return actions, state
    
def parse_start_state(contents):
    '''Takes a list of lines from the log and converts them into a state dict.
    
    Args:
        contents (list[str]): lines from the log file
    
    Returns:
        dict: describes the starting state of the game ('board', player numbers, 'starting_player')
    '''
    state = parse_state(contents, 1)
    state['starting_player'] = contents[0].split(':')[2]
    return state

def parse_end_state(contents):
    '''Takes a list of lines from the log and converts them into the final state dict.
    
    Args:
        contents (list[str]): lines from the log file
    
    Returns:
        dict: describes the ending state of the game ('board', player numbers, 'winner', 'value')
    '''

    state = parse_state(contents, 3)
    state['winner'] = int(contents[1].split(':')[-1])
    state['value'] = int(contents[2].split(':')[-1])
    return state

########################
# AUGMENTATION FUNCTIONS
########################

def reflect_tile(tile, horizontal, vertical):
    '''Converts tile coordinates into their reflected counterparts
    
    Args:
        tile (int or list[int] or np.ndarray): tile or list of tiles to be reflected
        horizontal (bool): whether to reflect across horizontal axis
        vertical (bool): whether to reflect across vertical axis
    
    Returns:
        int or list[int]: reflected tile numbers
    '''

    if isinstance(tile, (list,np.ndarray)):
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
    '''Permutes and reflects the game board.
    
    Args:
        board (np.ndarray): one hot encoded game board
        permutation (list[int]): permutation of the hotel chain labels
        horizontal (bool): whether to reflect across horizontal axis
        vertical (bool): whether to reflect across vertical axis
    
    Returns:
        np.ndarray: augmented game board
    '''

    board = np.take(board, permutation, axis=0)
    if horizontal:
        board = board[:,:,::-1]
    if vertical:
        board = board[:,::-1,:]
    return board
def permute_state(state, permutation, horizontal, vertical):
    '''Augment the state given the hotel listing permutation, as well as horizontal/vertical reflections of the board/tiles
    
    Args:
        orig_state (dict): dictionary describing the original state to be augmented
        permutation (list[int]): permutation of the hotel chain labels
        horizontal (bool): whether to reflect across horizontal axis
        vertical (bool): whether to reflect across vertical axis
    
    Returns:
        dict: augmented state dictionary
    '''
    state['board'] = permute_board(state['board'], permutation, horizontal, vertical)
    for i in range(4):
        state[i]['stocks'] = np.take(state[i]['stocks'], permutation[:-1])
        state[i]['tiles'] = reflect_tile(state[i]['tiles'], horizontal, vertical)
    return state

def permute_action(action, permutation, horizontal, vertical):
    '''Augment the action given the hotel listing permutation, as well as horizontal/vertical reflections of the tiles
    
    Args:
        action (dict): action dict to be augmented
        permutation (list[int]): permutation of the hotel chain labels
        horizontal (bool): whether to reflect across horizontal axis
        vertical (bool): whether to reflect across vertical axis
    
    Returns:
        dict: augmented action dictionary
    '''

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

def permute_gamelog(game_log, permutation, horizontal=False, vertical=False): 
    '''Construct an augmented variation of the game log under hotel permutations and board reflections.
    
    Args:
        game_log (dict): game log dict to be augmented
        permutation (list[int]): permutation of the hotel chain labels
        horizontal (bool): whether to reflect across horizontal axis
        vertical (bool): whether to reflect across vertical axis
    
    Returns:
        dict: augmented game log dictionary
    '''
    aug_log = copy.deepcopy(game_log)
    aug_log['start_state'] = permute_state(aug_log['start_state'], permutation, horizontal, vertical)
    for turn_id in range(len(game_log['turns'])):
        aug_log['turns'][turn_id]['state'] = permute_state(aug_log['turns'][turn_id]['state'], permutation, horizontal, vertical)
        aug_log['turns'][turn_id]['action'] = permute_action(aug_log['turns'][turn_id]['action'], permutation, horizontal, vertical)      
    aug_log['end_state'] = permute_state(aug_log['end_state'], permutation, horizontal, vertical)
    return aug_log

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
                        aug_log = permute_gamelog(game_log, permutation, horizontal, vertical)
                        game_logs.append(aug_log)
    return game_logs


if __name__ == '__main__':
    import sys
    import os
    
    if len(sys.argv) != 3:
        print("usage: python {} file_path save_dir")
    file_path = sys.argv[1]
    file_base = os.path.splitext(os.path.split(file_path)[-1])[0]

    save_base = os.path.join(sys.argv[2], file_base)
    game_log = parse_log(file_path)
    augmented_logs = construct_all_permuted_logs(game_log)

    import pickle
    with open('{}.pik'.format(save_base), 'wb') as f:
        pickle.dump(augmented_logs, f)
