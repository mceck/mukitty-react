const React = require('react');
const mukitty = require('./mukitty-react');

const Input = () => {
  const [value, setValue] = React.useState('x');
  return <input value={value} onChange={setValue} />;
};

const Composition = ({ children, height }) => {
  return (
    <row items={3} height={height} widths={[130, 130, 130]}>
      {children}
    </row>
  );
};

const App = () => {
  const [count, setCount] = React.useState(0);
  const [slider, setSlider] = React.useState(128);
  const [checked, setChecked] = React.useState(true);
  const clickMe = React.useCallback(() => {
    setCount(count + 1);
  }, [count]);
  const sliderRed = React.useMemo(() => slider << 16, [slider]);
  const sliderGreen = React.useMemo(() => slider << 8, [slider]);
  const sliderBlue = React.useMemo(() => slider, [slider]);
  return (
    <>
      <row items={2} height={50} widths={[200, -1]}>
        {!count && <label>Clicked ({count})</label>}
        <button onClick={clickMe}>Click</button>
      </row>
      <row items={2} height={20} widths={[200, -1]}>
        <label>Checkbox is {checked ? 'checked' : 'unchecked'}</label>
        <checkbox checked={checked} label="Check me!" onChange={setChecked} />
      </row>
      <row items={2} height={50} widths={[200, -1]}>
        <label>Input</label>
        <Input />
      </row>
      <row items={2} height={20} widths={[200, -1]}>
        <label>Slider value: {slider.toFixed(2)}</label>
        <slider min={0} max={255} value={slider} onChange={setSlider} />
      </row>
      <Composition height={80}>
        <rect color={sliderRed} />
        <rect color={sliderGreen} />
        <rect color={sliderBlue} />
      </Composition>
      <Composition>
        <col>
          <span>Composition 1a</span>
          <span>Composition 1b</span>
          <span>Composition 1c</span>
        </col>
        <span>Composition 2</span>
        <span>Composition 3</span>
        <span>Composition 4</span>
        <span>Composition 5</span>
        <span>Composition 6</span>
      </Composition>
    </>
  );
};

mukitty.render(<App />);
