export const COLOURS = [
  '#000000', '#0000ff', '#ff0000', '#ff00ff',
  '#00ff00', '#00aaff', '#ffff00', '#ffffff',
  '#808080', '#0055ff', '#aa0000', '#550000',
  '#00aa00', '#005500', '#ff8000', '#804000',
];

export const COLOUR_NAMES = [
  'Black', 'Blue', 'Red', 'Magenta',
  'Green', 'Light Blue', 'Yellow', 'White',
  'Mid Grey', 'Mid Blue', 'Mid Red', 'Dark Red',
  'Mid Green', 'Dark Green', 'Orange', 'Brown',
];

export const TILE_TYPES = [
  { value: 3,  name: 'Space' },
  { value: 4,  name: 'Solid' },
  { value: 5,  name: 'Floor' },
  { value: 6,  name: 'Collapse' },
  { value: 7,  name: 'Conveyor ←' },
  { value: 8,  name: 'Conveyor →' },
  { value: 0,  name: 'Item' },
  { value: 9,  name: 'Harm' },
  { value: 10, name: 'Void' },
  { value: 1,  name: 'Switch Off' },
  { value: 2,  name: 'Switch On' },
];

export const TYPE_BADGE = {
  0:  { label: 'ITEM',  bg: '#b8860b' },
  1:  { label: 'SW-',   bg: '#5555aa' },
  2:  { label: 'SW+',   bg: '#226622' },
  3:  { label: '',      bg: 'transparent' },
  4:  { label: 'SOLID', bg: '#555555' },
  5:  { label: 'FLOOR', bg: '#7a4a22' },
  6:  { label: 'FALL',  bg: '#cc5500' },
  7:  { label: '←',     bg: '#006666' },
  8:  { label: '→',     bg: '#660066' },
  9:  { label: 'HARM',  bg: '#880000' },
  10: { label: 'VOID',  bg: '#220022' },
};
