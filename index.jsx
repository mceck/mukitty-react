const React = require('react');
const mukitty = require('./mukitty-react');

const Composition = ({ children, height }) => {
  return (
    <row items={3} height={height} widths={[130, 130, 130]}>
      {children}
    </row>
  );
};

const Tree = () => {
  return (
    <tree title="Tree Node" startOpened={true}>
      <tree title="Nested Node" startOpened={true}>
        <tree title="Child Node" startOpened={true}>
          <row items={2} height={20} widths={[150, -1]}>
            <col>
              <span>Child 1</span>
              <span>Child 2</span>
              <span>Child 3</span>
            </col>
            <col>
              <tree title="Grandchild Node">
                <col>
                  <span>Grandchild 1</span>
                  <span>Grandchild 2</span>
                  <span>Grandchild 3</span>
                </col>
              </tree>
            </col>
          </row>
        </tree>
      </tree>
    </tree>
  );
};

const App = () => {
  const [count, setCount] = React.useState(0);
  const [slider, setSlider] = React.useState(128);
  const [checked, setChecked] = React.useState(true);
  const [inputValue, setInputValue] = React.useState('');
  const [isOpen, setIsOpen] = React.useState(false);
  const sliderRed = React.useMemo(() => slider << 16, [slider]);
  const sliderGreen = React.useMemo(() => slider << 8, [slider]);
  const sliderBlue = React.useMemo(() => slider, [slider]);
  return (
    <>
      <row items={2} height={50} widths={[200, -1]}>
        <label>Clicked ({count})</label>
        <button onClick={() => setCount(count + 1)}>Click</button>
      </row>
      <row items={2} height={20} widths={[200, -1]}>
        <label>Checkbox is {checked ? 'checked' : 'unchecked'}</label>
        <checkbox checked={checked} label="Check me!" onChange={setChecked} />
      </row>
      <row items={2} height={20} widths={[200, -1]}>
        <label>Input {inputValue}</label>
        <input value={inputValue} onChange={setInputValue} />
      </row>
      <row items={2} height={20} widths={[200, -1]}>
        <label>Slider value: {slider.toFixed(2)}</label>
        <slider min={0} max={255} value={slider} onChange={setSlider} />
      </row>
      <row items={1} height={20} widths={[-1]}>
        <button onClick={() => setIsOpen(!isOpen)}>
          {isOpen ? 'Close Modal' : 'Open Modal'}
        </button>
      </row>
      {isOpen && (
        <modal
          title="Modal Title"
          top={100}
          left={100}
          width={200}
          height={100}
          onClose={() => setIsOpen(false)}
        >
          <row items={1} height={20} widths={[-1]}>
            <label>Modal content</label>
          </row>
        </modal>
      )}
      <header title="Collapsible" startOpened={true}>
        <Composition height={80}>
          <rect color={sliderRed} />
          <rect color={sliderGreen} />
          <rect color={sliderBlue} />
        </Composition>
      </header>

      <Tree />
    </>
  );
};

mukitty.render(<App />);
