import { readFileSync } from "fs";
import { exec } from "child_process";

function chunk(arr, chunkSize) {
  const res = [];
  while (arr.length > 0) {
    const chunk = arr.splice(0, chunkSize);
    res.push(chunk);
  }
  return res;
}

let file = readFileSync(".gitmodules.bad")
  .toString()
  .split("\n")
  .filter((_, index) => {
    return index % 3 !== 0;
  })
  .map((x) => x.split("=")[1].trim());
file = chunk(file, 2);

file.forEach((x) => {
  console.log(`git submodule add ${x[1]} ${x[0]}`);
});
