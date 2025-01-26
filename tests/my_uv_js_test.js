setTimeout(() => {
  print('Hello from setTimeout after 1 second');
  Promise.resolve().then(() => {
    print('Running Promise.resolve after 1 second');
  });
}, 1000);
Promise.resolve().then(() => {
  print('after running Promise.resolve')
});
setTimeout(() => {
  print('Hello from setTimeout after 2 second');
}, 2000);
print('After setTimeout');