const mukitty = require('./build/Release/mukitty.node');

mukitty.init();
while (true) {
  const stop = mukitty.handleInputs();
  if (stop) break;
  mukitty.begin();
  mukitty.beginWindow('root');
  {
    mukitty.layoutRow(30, 200);
    {
      mukitty.label('Click', 12);
      if (mukitty.button('Click me', 12)) {
        console.log('Button clicked');
      }
    }
    mukitty.endLayout();
  }
  mukitty.endWindow();
  mukitty.end();
}
mukitty.close();
