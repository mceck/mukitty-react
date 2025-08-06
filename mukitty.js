const mu = require('./build/Release/mukitty.node');

mu.init();
try {
  let sliderValue = 128;
  while (true) {
    const stop = mu.updateInput();
    if (stop) break;
    mu.begin();
    mu.beginWindow('Mukitty');
    mu.layoutRow(2, 50, 150, -1);
    mu.button('Button');
    sliderValue = mu.slider(0, 255, sliderValue);
    mu.endWindow();
    mu.end();
  }
} catch (e) {
  console.error('Error:', e);
}
mu.close();
