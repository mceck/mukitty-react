const React = require('react');
const mukitty = require('./mukitty-react');

const Composition = ({ children, height }) => {
  return (
    <row height={height} widths={[130, 130, 130]}>
      {children}
    </row>
  );
};

const Tree = () => {
  return (
    <tree title="Tree Node" startOpened={true}>
      <tree title="Nested Node" startOpened={true}>
        <tree title="More Nested Node" startOpened={true}>
          <row height={20} widths={[150, -1]}>
            <col>
              <text>Child 1</text>
              <text>Child 2</text>
              <text>Child 3</text>
            </col>
            <col>
              <tree title="Another Tree...">
                <col>
                  <text>Item 1</text>
                  <text>Item 2</text>
                  <text>Item 3</text>
                </col>
              </tree>
            </col>
          </row>
        </tree>
      </tree>
    </tree>
  );
};

const Modal = ({ onClose }) => {
  const [log, setLog] = React.useState([]);
  const [msg, setMsg] = React.useState('');

  const onSubmit = React.useCallback(() => {
    setLog([...log, msg]);
    setMsg('');
  }, [log, msg]);

  return (
    <modal
      title="Modal Title"
      top={100}
      left={100}
      width={200}
      height={150}
      onClose={onClose}
    >
      <row height={-25} widths={[-1]}>
        <panel title="Modal Panel">
          <row height={0} widths={[-1]}>
            {log.map((val, i) => (
              <text key={i}>{val}</text>
            ))}
          </row>
        </panel>
      </row>
      <row height={20} widths={[150, -1]}>
        <input value={msg} onChange={setMsg} onSubmit={onSubmit} />
        <button onClick={onSubmit}>Send</button>
      </row>
    </modal>
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
      <row height={50} widths={[200, -1]}>
        <label>Clicked ({count})</label>
        <button onClick={() => setCount(count + 1)}>Click</button>
      </row>
      <row height={20} widths={[200, -1]}>
        <label>Checkbox is {checked ? 'checked' : 'unchecked'}</label>
        <checkbox checked={checked} label="Check me!" onChange={setChecked} />
      </row>
      <row height={20} widths={[200, -1]}>
        <label>Input {inputValue}</label>
        <input value={inputValue} onChange={setInputValue} />
      </row>
      <row height={20} widths={[200, -1]}>
        <label>Slider value: {slider.toFixed(2)}</label>
        <slider min={0} max={255} value={slider} onChange={setSlider} />
      </row>
      <row height={20} widths={[-1]}>
        <button onClick={() => setIsOpen(!isOpen)}>
          {isOpen ? 'Close Modal' : 'Open Modal'}
        </button>
      </row>
      {isOpen && <Modal onClose={() => setIsOpen(false)} />}
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
