// Deterministic promotional illustration, not a captured application window.
// Requires Node.js and sharp. Run: node tools/render-readme.cjs
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
const appearance = fs.readFileSync(path.join(root,'src','appearance.h'),'utf8');
const opacity = Number(appearance.match(/kBackgroundOpacity\s*=\s*([0-9.]+)f/)[1]);
const data = buffer => 'data:image/png;base64,' + buffer.toString('base64');
const asset = name => data(fs.readFileSync(path.join(root,'assets',name+'-ui.png')));
async function darkTaskbar(source) {
 const image=await sharp(source).ensureAlpha().raw().toBuffer({resolveWithObject:true});
 for(let i=0;i<image.data.length;i+=4) {
  const r=image.data[i],g=image.data[i+1],b=image.data[i+2];
  const high=Math.max(r,g,b),low=Math.min(r,g,b),saturation=high?(high-low)/high:0,luma=(r+g+b)/765;
  if(saturation<.14) {
   const value=luma>.58?Math.round(25+(1-luma)*28):Math.round(246-luma*80);
   image.data[i]=image.data[i+1]=image.data[i+2]=value;
  }
 }
 return sharp(image.data,{raw:image.info}).png().toBuffer();
}
async function scene(light) {
const traySource=path.join(root,'docs','images','windows11-taskbar.png');
const trayCrop=await sharp(traySource).extract({left:985,top:0,width:393,height:98}).png().toBuffer();
const trayBuffer=light
 ? trayCrop
 : await darkTaskbar(trayCrop);
const modeIcon=fs.readFileSync(path.join(root,'assets','tray','balanced.svg'),'utf8')
 .replaceAll('#212121',light?'#202124':'#f5f7fa');
const wallpaper = 'data:image/png;base64,' + fs.readFileSync(path.join(root,'docs','images',light?'wallpaper-light.png':'wallpaper.png')).toString('base64');
const background = `<image href="${wallpaper}" width="900" height="900" preserveAspectRatio="xMidYMid slice"/>`;
const txt=(x,y,value,size=13,color='#24272d',anchor='start',weight=400)=>
 `<text x="${x}" y="${y}" font-size="${size}" fill="${color}" text-anchor="${anchor}" font-weight="${weight}">${value}</text>`;
const rail=(y,n,selected,labels)=>{
 const x=40+312*selected/(n-1);
 return `<rect x="40" y="${y-2}" width="312" height="4" rx="2" fill="#536273" opacity=".40"/>
 <rect x="40" y="${y-2}" width="${x-40}" height="4" rx="2" fill="#0066bf"/>
 <circle cx="${x}" cy="${y+1}" r="10.5" fill="#000" opacity=".12"/>
 <circle cx="${x}" cy="${y}" r="10" fill="#fff"/>
 <circle cx="${x}" cy="${y}" r="5" fill="#0066bf"/>`+
 labels.map((label,i)=>txt(i===0?28:i===n-1?364:40+312*i/(n-1),y+28,label,10,'#535a64',i===0?'start':i===n-1?'end':'middle')).join('');
};
const card=(top,name,icon,status,active,n,selected,labels)=>`
 <rect x="16" y="${top}" width="360" height="112" rx="8" fill="#fff" fill-opacity=".22" stroke="#000" stroke-opacity=".055"/>
 <image href="${asset(icon)}" x="24" y="${top+11}" width="34" height="34"/>
 ${txt(62,top+31,name)}${txt(354,top+30,status,11,active?'#0066bf':'#666b74','end')}
 ${rail(top+62,n,selected,labels)}`;
const taskbar=light=>{
 const background=light?'#f1f1f1':'#181818';
 return `<g>
  <rect x="0" y="836" width="900" height="64" fill="${background}"/>
  <path d="M0 836H900" stroke="${light?'#000':'#fff'}" stroke-opacity=".1"/>
  <image href="${data(trayBuffer)}" x="643" y="836" width="257" height="64" preserveAspectRatio="none"/>
  <image href="data:image/svg+xml;base64,${Buffer.from(modeIcon).toString('base64')}" x="615" y="856" width="22" height="22"/>
 </g>`;
};
let svg=`<svg xmlns="http://www.w3.org/2000/svg" width="900" height="900" viewBox="0 0 900 900">
<defs>
 <filter id="blur" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="25"/></filter>
 <filter id="shadow" x="-40%" y="-40%" width="180%" height="180%"><feDropShadow dx="0" dy="26" stdDeviation="28" flood-color="#152348" flood-opacity=".3"/></filter>
 <clipPath id="panel"><rect x="77.6" y="98.5" width="744.8" height="703" rx="18"/></clipPath>
</defs>
${background}
<g clip-path="url(#panel)"><g filter="url(#blur)">${background}</g>
<rect x="77.6" y="98.5" width="744.8" height="703" fill="#f2f2f2" fill-opacity="${opacity}"/></g>
<rect x="77.6" y="98.5" width="744.8" height="703" rx="18" fill="none" stroke="#fff" stroke-opacity=".7"/>
<g transform="translate(77.6 98.5) scale(1.9)" font-family="Segoe UI,Arial,sans-serif">
${txt(24,35,'Power mode',16,'#20242a','start',600)}
${card(58,'Plugged in','power','Balanced',true,3,1,['Efficiency','Balanced','Performance'])}
${card(182,'On battery','battery','Efficiency',false,3,0,['Efficiency','Balanced','Performance'])}
<rect x="16" y="306" width="360" height="48" rx="8" fill="#fff" fill-opacity=".22"/>
<image href="${asset('coffee')}" x="24" y="313" width="34" height="34"/>
${txt(62,335,'Keep awake')}
<rect x="312" y="320" width="40" height="20" rx="10" fill="#ffffff" fill-opacity=".1" stroke="#6b7079"/>
<circle cx="322" cy="330" r="6" fill="#6b7079"/>
</g>
${taskbar(light)}
</svg>`;

if(!light) svg=svg.replaceAll('#24272d','#f5f5f7').replaceAll('#20242a','#f5f5f7')
 .replaceAll('#535a64','#a1a1a8').replaceAll('#666b74','#a1a1a8').replaceAll('#6b7079','#a1a1a8')
 .replaceAll('#0066bf','#66c2ff').replaceAll('#f2f2f2','#1f1f1f')
 .replaceAll('fill-opacity=".22"','fill-opacity=".035"')
 .replaceAll('stroke="#000" stroke-opacity=".055"','stroke="#fff" stroke-opacity=".075"')
 .replaceAll('stroke-opacity=".7"','stroke-opacity=".12"')
 .replaceAll('fill="#536273" opacity=".40"','fill="#fff" opacity=".38"');
else svg=svg.replaceAll('fill="#536273" opacity=".40"','fill="#000" opacity=".30"');
return sharp(Buffer.from(svg)).png().toBuffer();
}
(async()=>{
 const [dark,light]=await Promise.all([scene(false),scene(true)]);
 await sharp({create:{width:1800,height:900,channels:4,background:'#000'}})
 .composite([{input:dark,left:0,top:0},{input:light,left:900,top:0}])
 .png().toFile(path.join(root,'docs','images','preview.png'));
 console.log('Created preview.png: dark and light, 1800 x 900');
})().catch(error=>{console.error(error);process.exitCode=1;});
