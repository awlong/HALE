import numpy as np
import copy

class PermutableObject:
    '''
    Defines a default permutation function for all objects that can be overridden for base types,
    as well as a default representation function.
    '''
    is_valid = False
    def __repr__(self):
        if self.is_valid:
            tmp = dict(self.__dict__)
            tmp.pop('is_valid')
            return tmp.__repr__()
        else:
            return ''
    def permute(self, perm=None, horizontal=False, vertical=False):
        for key in self.__dict__.keys():
            if isinstance(self.__dict__[key], PermutableObject):
                self.__dict__[key].permute(perm, horizontal, vertical)
            if isinstance(self.__dict__[key], list):
                for index in range(len(self.__dict__[key])):
                    if isinstance(self.__dict__[key][index], PermutableObject):
                        self.__dict__[key][index].permute(perm, horizontal, vertical)

class Parsable:
    '''
    Defines an interface for all objects that are directly parsed from the log file
    '''

    def parse_contents(self, contents):
        raise NotImplementedError('Parsing contents not implemented')


class Board(PermutableObject, Parsable):
    '''Board is a one-hot encoded variant of the game board, each layer is one of the possible states 
    defined in the STATE_MAP (each hotel, with the final layer being the non-chain active tiles)
    
    Fields:
        layers (np.ndarray[8, 9, 12]): one hot encoding of the game state into layers
    '''

    def __init__(self):
        self.layers = np.zeros((8, 9, 12), np.uint8)
    
    def parse_contents(self, contents):
        self.layers[:] = 0
        for i, line in enumerate(contents):
            states = line.split(' ')
            for j, state in enumerate(states[:-1]):
                if state != '.':
                    self.layers[GameHistory.STATE_MAP[state], i, j] = 1
        self.is_valid = True

    def permute(self, perm, horizontal, vertical):
        if perm:
            self.layers = np.take(self.layers, perm, axis=0)
        if horizontal:
            self.layers = self.layers[:, :, ::-1]
        if vertical:
            self.layers = self.layers[:, ::-1, :]

class Chain(PermutableObject):
    '''Chain is a wrapper around the hotel number so that it can be permuted
    
    Fields:
        hotel_id (int): the hotel descriptor
    '''
    def __init__(self, hotel_id=None):
        self.hotel_id = hotel_id
        self.is_valid = True
    def permute(self, perm, horizontal, vertical):
        if perm:
            self.hotel_id = perm[self.hotel_id]

class Stock(Chain):
    '''Stock is a Chain with an associated count
    
    Fields:
        hotel_id (int): the hotel descriptor
        count (int): the number of shares
    '''

    def __init__(self, hotel_id=None, count=None):
        self.hotel_id = hotel_id
        self.count = count
        self.is_valid = True

class Tile(PermutableObject):
    '''Tile object is a wrapper around the tile number so that it can be permuted/reflected.

    Fields:
        tile (int): raw tile value
    '''

    def __init__(self, tile=255):
        self.tile = int(tile)
        self.is_valid = True
    def permute(self, perm, horizontal, vertical):
        if self.tile == 255 or (not horizontal and not vertical):
            return
        elif horizontal and not vertical:
            self.tile = GameHistory.HORIZ_BOARD[self.tile]
        elif not horizontal and vertical:
            self.tile = GameHistory.VERT_BOARD[self.tile]
        else:
            self.tile = GameHistory.HORIZ_VERT_BOARD[self.tile]

class Player(PermutableObject, Parsable):
    '''Player state
    
    Fields:
        name (str): name string of the player
        cash (int): on-hand cash
        tiles (list[Tile]): list of Tile objects in the player's hand
        stocks (list[Stock]): list of owned stocks
        value (int): total valuation
    '''

    def __init__(self, name=None, cash=None, tiles=None, stocks=None, value=None):
        self.name = name
        self.cash = cash
        self.tiles = tiles
        self.stocks = stocks
        self.value = value

    def parse_contents(self, contents):
        self.name = contents[0][10:-1]
        self.cash = int(contents[1][1:])
        self.tiles = [Tile(i) for i in contents[2].strip().split(' ')[1:]]
        self.stocks = [Stock(hotel,count) for hotel,count in enumerate(contents[3].strip().split(' ')[1:])]
        self.value = int(contents[4].split('$')[1])
        self.is_valid = True

class GameState(PermutableObject, Parsable):
    '''Defines the board and player states
    
    Fields:
        board (Board): the game board state
        players (list[Player]): a list of each player's current state
    '''

    def __init__(self, board=None, players=None):
        if board:
            self.board = board
        else:
            self.board = Board()
        if players:
            self.players = players
        else:
            self.players = [Player(), Player(), Player(), Player()]

    def parse_contents(self, contents):
        self.board.parse_contents(contents[:9])
        for i in range(4):
            start = 10+5*i
            end = start+5
            self.players[i].parse_contents(contents[start:end])
        self.is_valid = True

class MergeAction(PermutableObject, Parsable):
    '''The actions taken during a merger

    Fields:
        chains (list[Chain]): the chains involved in the merger
        survivor (Chain): the chain that survives the merger
        player_order (list[int]): the order of players traversed in the merge
        trades (list[int]): the number of surviving stocks acquired via trading
        sales (list[int]): the number of defunct stocks that were traded
    '''

    def __init__(self, chains=None, survivor=None, player_order=None, trades=None, sales=None):
        self.chains = chains
        self.survivor = survivor
        self.player_order = player_order
        self.trades = trades
        self.sales = sales

    def parse_contents(self, contents):
        merger_lines = [line for line in contents if 'handleTilePlayMerger' in line]
        if len(merger_lines) == 0:
            self.is_valid = False
            return
        self.chains = []
        for line in merger_lines:
            if 'Merging Chains' in line:
                self.chains.append(Chain(GameHistory.CHAIN_MAP[line.split(':')[-1].strip()]))
            if 'Surviving chain' in line:
                self.survivor = Chain(GameHistory.CHAIN_MAP[line.split(':')[-1].strip()])
        merge_trade_index = [i for i,line in enumerate(contents) if 'Player Merge Actions:' in line]
        self.player_order = []
        self.trades = []
        self.sales = []
        for start_index in merge_trade_index:
            self.player_order.append(int(contents[start_index].split(':')[-1]))
            self.sales.append(int(contents[start_index+1].split(':')[-1]))
            self.trades.append(int(contents[start_index+2].split(':')[-1]))
        self.is_valid = True

class CreateAction(PermutableObject, Parsable):
    '''The chain creation action
    
    Fields:
        chain (Chain): the chain to create
        share (bool): whether a stock was given to the creating player
    '''

    def __init__(self, chain=None, share=False):
        self.chain = chain
        self.share = share
    
    def parse_contents(self, contents):
        create_lines = [line for line in contents if 'Create' in line]
        if len(create_lines) == 0:
            self.is_valid = False
            return
        self.chain = Chain(GameHistory.CHAIN_MAP[create_lines[0].split(':')[-1].strip()])
        if len(create_lines) == 2:
            self.share = True
        else:
            self.share = False
        self.is_valid = True

class ShareAction(PermutableObject, Parsable):
    ''' Share purchase action
    
    Fields:
        stocks (list[Stock]): list of stocks (Stock.hotel_id, Stock.count) that are purchased
    '''

    def __init__(self, stocks=None):
        self.stocks = stocks
    
    def parse_contents(self, contents):
        self.stocks = []
        share_purchase_lines = [line for line in contents if 'SharePurchasePhase' in line]
        if len(share_purchase_lines) == 0:
            self.is_valid = False
            return
        for line in share_purchase_lines:
            tokens = line.split(':')
            stock_id = GameHistory.CHAIN_MAP[tokens[1].strip()]
            count = int(tokens[2])
            self.stocks.append(Stock(stock_id, count))
        self.is_valid = True

class Action(PermutableObject, Parsable):
    '''The set of actions taken for a given turn
    
    Fields:
        current_player (int): player who performed the action
        tile (Tile): the tile object that was placed
        merge (MergeAction): the merging chain action
        create (CreateAction): the creating a chain action
        share (ShareAction): the share purchasing action
    '''

    def __init__(self, current_player=0, tile=None, merge=None, create=None, share=None):
        self.current_player = current_player
        if tile:
            self.tile = tile
        else:
            self.tile = Tile()
        if merge:
            self.merge = merge
        else:
            self.merge = MergeAction()
        if create:
            self.create = create
        else:
            self.create = CreateAction()
        if share:
            self.share = share
        else:
            self.share = ShareAction()

    def parse_contents(self, contents):
        self.parse_base_action(contents)
        self.merge.parse_contents(contents)
        self.create.parse_contents(contents)
        self.share.parse_contents(contents)
        self.is_valid = True

    def parse_base_action(self, contents):
        self.current_player = int([line.split(':')[-1] for line in contents if 'Player Number' in line][0])
        self.tile = Tile(int([line.split(':')[-1] for line in contents if 'Playing tile' in line][0]))


class Turn(PermutableObject, Parsable):
    '''Describes an individual turn (the action taken and the resulting game state)
    
    Fields:
        action (Action): action taken on the turn
        state (GameState): state after the action was taken
    '''

    def __init__(self, action=None, state=None):
        if action:
            self.action = action
        else:
            self.action = Action()
        if state:
            self.state = state
        else:
            self.state = GameState()

    def parse_contents(self, contents):
        state_start = [i+1 for i,line in enumerate(contents) if 'canEndGame' in line][0]
        self.action.parse_contents(contents[:state_start])
        self.state.parse_contents(contents[state_start:])
        self.is_valid = True

class GameHistory(PermutableObject, Parsable):
    '''GameHistory object describes the full set of game states/actions for a game
    
    Fields:
        start_state (GameState): initial state of the game
        end_state (GameState): final state of the game
        turns (list(Turn)): list of Turn objects (Turn.action, Turn.state)
        start_player (int): player that started
        winner (int): player that won
        value (int): cash value of the winner
    '''

    HORIZ_BOARD = np.arange(9*12).reshape(9,12)[:,::-1].flatten()
    VERT_BOARD = np.arange(9*12).reshape(9,12)[::-1,:].flatten()
    HORIZ_VERT_BOARD = np.arange(9*12).reshape(9,12)[::-1,::-1].flatten()
    STATE_MAP = {'L':0, 'T':1, 'W':2, 'A':3, 'F':4, 'I':5, 'C':6, '#':7}
    CHAIN_MAP = {'LUXOR':0, 0:'LUXOR', 'TOWER':1, 1:'TOWER', 
                 'WORLDWIDE':2, 2:'WORLDWIDE', 'AMERICAN':3, 3:'AMERICAN', 
                 'FESTIVAL':4, 4:'FESTIVAL', 'IMPERIAL':5, 5:'IMPERIAL', 
                 'CONTINENTAL':6, 6:'CONTINENTAL', 'UNCONNECTED':7, 7:'UNCONNECTED'}
    def __init__(self, file_path):
        with open(file_path, 'r') as f:
            file_contents = f.read()
        line_contents = file_contents.splitlines()
        self.parse_contents(line_contents)
        
    def parse_contents(self, contents):
        # identify turn starts to block the different states
        turn_starts = [i for i,line in enumerate(contents) if line == 'TURN START' or line == 'runGame: END OF GAME']

        self.parse_start_state(contents[:turn_starts[0]])
        self.parse_turns(contents, turn_starts)
        self.parse_end_state(contents[turn_starts[-1]:])
        self.is_valid = True

    def parse_start_state(self, contents):
        self.start_state = GameState()
        self.start_state.parse_contents(contents[1:])
        self.start_player = contents[0].split(':')[2]

    def parse_turns(self, contents, starts):
        self.turns = []
        for i in range(len(starts)-1):
            turn = Turn()
            turn.parse_contents(contents[starts[i]:starts[i+1]])
            self.turns.append(turn)
    
    def parse_end_state(self, contents):
        self.end_state = GameState()
        self.end_state.parse_contents(contents[3:])
        self.winner = int(contents[1].split(':')[-1])
        self.value = int(contents[2].split(':')[-1])

def construct_all_permuted_logs(base_game):
    # permute the L/T - [0,1], [1,0] (2)
    # permute the W/A/F - [2,3,4],[2,4,3],[3,2,4],[3,4,2],[4,2,3],[4,3,2] (6)
    # permute the I/C - [5,6], [6,5] (2)
    # reflect left/right - (2)
    # reflect top/bottom - (2)
    games = []
    for LT in [[0,1],[1,0]]:
        for WAF in [[2,3,4],[2,4,3],[3,2,4],[3,4,2],[4,2,3],[4,3,2]]:
            for IC in [[5,6], [6,5]]:
                permutation = LT+WAF+IC+[7]
                for horizontal in [False, True]:
                    for vertical in [False, True]:
                        game = copy.deepcopy(base_game)
                        game.permute(permutation, horizontal, vertical)
                        games.append(game)
    return games


if __name__ == '__main__':
    import sys
    import os
    import pickle

    if len(sys.argv) != 3:
        print("usage: python {} file_path save_dir")
    file_path = sys.argv[1]
    file_base = os.path.splitext(os.path.split(file_path)[-1])[0]

    save_base = os.path.join(sys.argv[2], file_base)
    gh = GameHistory(file_path)
    games = construct_all_permuted_logs(gh)

    
    with open('{}.pik'.format(save_base), 'wb') as f:
        pickle.dump(games, f)