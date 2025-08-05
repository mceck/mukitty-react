const React = require('react');
const mukitty = require('./mukitty-react');

const App = () => {
  const [count, setCount] = React.useState(0);
  const [slider, setSlider] = React.useState(0);
  const [checked, setChecked] = React.useState(true);
  const [inputValue, setInputValue] = React.useState('x');
  const clickMe = () => {
    setCount(count + 1);
  };
  return (
    <>
      <layout items={2} height={50} widths={[200, -1]}>
        <label>Clicked ({count})</label>
        <button onClick={clickMe}>Click</button>
      </layout>
      <layout items={2} height={50} widths={[200, -1]}>
        <label>Slider value: {slider.toFixed(2)}</label>
        <slider min={0} max={10} value={slider} onChange={setSlider} />
      </layout>
      <layout items={2} height={50} widths={[200, -1]}>
        <label>Checkbox is {checked ? 'checked' : 'unchecked'}</label>
        <checkbox checked={checked} label="Check me!" onChange={setChecked} />
      </layout>
      <layout items={2} height={50} widths={[200, -1]}>
        <label>Input value: {inputValue}</label>
        <input value={inputValue} onChange={setInputValue} />
      </layout>
    </>
  );
};

mukitty.render(<App />);
