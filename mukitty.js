const mu = require('./build/Release/mukitty.node');

mu.init();
try {
  while (true) {
    const stop = mu.updateInput();
    if (stop) break;
    mu.begin();
    mu.beginWindow('Mukitty');
    // mu.layoutRow(1, 50, 380);
    mu.button('This is MuKitty!');
    mu.endWindow();
    mu.end();
  }
} catch (e) {
  console.error('Error:', e);
}
mu.close();
