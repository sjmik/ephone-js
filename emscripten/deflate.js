/* prettier-ignore */
const deflate = a=>{if(a==null)return"";const m=String,r=m.hasOwnProperty,t=m.fromCharCode,u="".charCodeAt,k={},v={},x=[];var c=a.byteLength,h=c&1,d=0;const n=[m(h)];for(;d<c>>1;d++)n.push(t(a[d*2]*256+a[d*2+1]));h&&n.push(t(a[c-1]*256));let b=c=a="",p=2,f=2,q=0,w=0;h=3;const e=(l,y=0)=>{for(let z=0;z<l;z++)q=q<<1|y&1,w==14?(w=0,x.push(t(q+32)),q=0):w++,y>>=1};d=()=>{r.call(v,b)?(u.call(b,0)<256?(e(f),e(8,u.call(b,0))):(e(f,1),e(16,u.call(b,0))),--p||(p=1<<f++),delete v[b]):e(f,k[b]);--p||(p=1<<f++)};for(let l=0;l<n.length;l+=1)a=n[l],r.call(k,a)||(k[a]=h++,v[a]=!0),c=b+a,r.call(k,c)?b=c:(d(),k[c]=h++,b=m(a));return b!==""&&d(),e(f,2),e(14),x.join("")+" "};

/* prettier-ignore */
const inflate = p=>{if(!p)return null;const m=[],r=[],u=[-1,-1,-1,-1];let q=p.charCodeAt(0)-32,c=16384,v=1,a=0,d=4,b=4,e=3,f=256,h=3,n,l;if(q&c&&(f*=f),c>>=1,q&c)return null;c>>=1;var k=w=>{let t=1;for(a=0;t!=w;){q&c&&(a|=t);c>>=1;if(!c){c=16384;q=p.charCodeAt(v++)-32}t<<=1}};k(f);f=[0,0,0,a];m.push(a);for(;;){if(v>p.length)return null;if(k(1<<e),a<2)k(a?65536:256),f[b]=a,u[b]=-1,a=b++,--d==0&&(d=1<<e++);else if(a===2){d=m.length-1;b=d*2-+(m[0]===49);e=new Uint8Array(b);for(h=0;h<d;){k=h*2;n=m[++h];e[k]=n>>>8;k+1<b&&(e[k+1]=n&255)}return e}r.length=0;for(l=a===b?h:a;l!==-1;)n=f[l],r.push(n),l=u[l];for(l=r.length;l--;)m.push(r[l]);a===b&&m.push(n);f[b]=n;u[b++]=h;h=a;--d==0&&(d=1<<e++)}};

const log = (message) => process.stderr.write(message + '\n');

const input = process.argv[2];
if (!input) {
  console.error('No input!');
  log(
    `Usage: ${process.argv[0]?.split('/').at(-1) ?? 'node'} ${process.argv[1]?.split('/').at(-1) ?? 'deflate.js'} "path-to-file-to-deflate"`
  );
  process.exit(1);
}

log(`Read: ${input}`);
import { readFileSync } from 'node:fs';
const contents = await readFileSync(input);

const squished = deflate(contents);
const sanityCheck = inflate(squished);
if (!contents.equals(sanityCheck)) {
  console.error(`Compression round trip failed! [${contents.length}] [${sanityCheck?.length}]`);
  process.exit(1);
}
log(`  in=[${contents.byteLength}] out=[${squished.length}]`);

/* const data = `${squished}`; */
const templateSafe = squished.replace(/\\/g, '\\\\').replace(/`/g, '\\`').replace(/\$\{/g, '\\${');
process.stdout.write(templateSafe);
